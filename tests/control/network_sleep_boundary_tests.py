"""Integration guards for the actual owner lifecycle and protected policy seams."""
from pathlib import Path
r=Path(__file__).resolve().parents[2]
n=(r/"components/network_manager/runtime.cpp").read_text()
p=(r/"components/power_management/runtime.cpp").read_text()
s=n.split("auto sleep_stop =",1)[1].split("struct SleepPort",1)[0]
checks=[]
def check(v):
    checks.append(v)
    assert v
check("esp_wifi_stop()" in s and "esp_wifi_start()" in s)
check("connect();publish();" in s and "configure_sta" not in s)
check(not any(x in s for x in ("nvs_", "erase(", "replace(", "open_setup(", "start_portal(")))
check(s.index("driver_events.store(false)")<s.index("esp_wifi_stop()"))
check(s.index("fence()")<s.index("driver_events.store(true)"))
check("inbox.seal()" in s and "inbox.unseal()" in s)
check("if(!driver_events.load())return" in n)
check("m.generation == policy.generation" in n)
check("m.detail!=65536 || policy.state==State::CONNECTED" in n and "&& !healthy()" in n)
core_gate=p.index("core_refused.store(false); request.store(Request::SLEEP)")
network_gate=p.index("network_sleep_epoch=network_manager::sleep_prepare()")
check(core_gate<network_gate)
check(p.index("if (!wait_flag(core_parked, true, 500) || core_refused.load())",core_gate)<network_gate)
check("request.store(Request::NONE); machine.refuse_sleep(now);" in p[core_gate:network_gate])
check("network_deadline=time_now()+10000" in p and "vTaskDelay(pdMS_TO_TICKS(10))" in p)
check("network_manager::sleep_resume(network_sleep_epoch)" in p)
check("!ui_pause.load()" in p and "!network_manager::sleep_ready(network_sleep_epoch)" in p)
check("sleep_timer.configure(forced_sleep" in p)
check("physical_activity.load()!=seen_activity" in p)
check('"POWER", 4096' in p and '"NETWORK", 6144' in n)
check(p.count("controlled_release(board,")==1 and "case '!':" in p)
check("esp_sleep_enable_gpio_wakeup()" in p)
check("publish_power(" in p)
check("return !web_stats().http" in s and "return !dns_active()" in s)
check('esp_event_post(REMOTE_SLEEP_EVENTS' in n and "sleep_barrier_ack.load()!=ticket" in n)
print(f"NETWORK_SLEEP_BOUNDARY: PASS=1 FAIL=0 assertions={len(checks)}")

