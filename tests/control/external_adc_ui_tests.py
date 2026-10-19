"""Execute actual GPIO1 ADC functions and HOME icon branch against host seams."""
from pathlib import Path
import os, re, subprocess, tempfile
root = Path(__file__).resolve().parents[2]
hardware = (root / "components/power_management/hardware.cpp").read_text()
external = "void Board::initialize_external()" + hardware.split("void Board::initialize_external()", 1)[1].split("bool Board::input(", 1)[0]
battery = "bool Board::battery(" + hardware.split("bool Board::battery(", 1)[1].split("static_assert(", 1)[0]
ui = (root / "components/user_app/user_app.cpp").read_text()
branch = "const bool plugged=power_management::external_plugged_in" + ui.split("const bool plugged=power_management::external_plugged_in", 1)[1].split("control_home::Snapshot s;", 1)[0]
assert ui.index("external_plugged_in") < ui.index("if (!control_home::read")
assert "LV_SYMBOL_USB, &lv_font_montserrat_12, text, 104, 10, 16" in ui
assert "home.battery_icon = remote01_panel(page, 104, 13, 13, 8" in ui
assert "home.battery_tip = remote01_panel(page, 117, 15, 2, 4" in ui
assert hardware.index("initialize_external();") > hardware.index("Wom::configure(*this)")
assert "ADC1_CHANNEL_0_GPIO_NUM==1 && ADC1_CHANNEL_3_GPIO_NUM==4" in hardware
font=(root / "components/lvgl/src/font/lv_font_montserrat_12.c").read_text(encoding="utf-8")
unicode=[int(n,16) for n in re.findall(r"0x[0-9a-f]+",font.split("unicode_list_1[] = {")[1].split("};")[0])]
glyph=96+unicode.index(0xf287-176)
row=re.findall(r"\{[^{}]+\}",font.split("glyph_dsc[] = {")[1].split("};")[0])[glyph]
assert ".box_w = 16" in row and ".adv_w = 240" in row
assert ".line_height = 15" in font
assert 104+16<=122 and 10+15<32
runtime = (root / "components/power_management/runtime.cpp").read_text()
assert "sample_external(board,now)" in runtime
assert "machine.tick(now, sleep_hardware && real.valid && hold, real.external)" in runtime
assert "external_published=external_measurement.view" in runtime
source = r'''
#include "power_management/external_power.hpp"
#include <cstdio>
#include <initializer_list>
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_ERR_INVALID_STATE=2,ESP_ERR_INVALID_RESPONSE=3;
constexpr int ADC_ATTEN_DB_12=3,ADC_BITWIDTH_12=12,ADC_UNIT_1=0,ADC_CHANNEL_0=0,ADC_CHANNEL_3=3;
struct adc_oneshot_chan_cfg_t {int atten=0,bitwidth=0;};
struct adc_cali_curve_fitting_config_t {int unit_id=0,chan=0,atten=0,bitwidth=0;};
static unsigned assertions=0,reads=0,conversions=0,creates=0,read_fail=0;
static int config_error=0,create_error=0,convert_error=0,value=1000,node_mv=2420,last_channel=-1;
static void *last_calibration=nullptr;
#define CHECK(x) do{++assertions;if(!(x)){std::printf("FAIL line=%u\n",__LINE__);return 1;}}while(0)
static int adc_oneshot_config_channel(void*p,int ch,adc_oneshot_chan_cfg_t*c) {
 if(!p || ch!=0 || c->atten!=3 || c->bitwidth!=12)return -9;
 return config_error;
}
static int adc_cali_create_scheme_curve_fitting(adc_cali_curve_fitting_config_t*c,void**p) {
 ++creates;if(c->unit_id!=0 || c->chan!=0 || c->atten!=3 || c->bitwidth!=12)return -9;
 if(create_error)return create_error;
 *p=reinterpret_cast<void*>(2);return 0;
}
static int adc_oneshot_read(void*,int channel,int*out) {
 ++reads;last_channel=channel;if(read_fail && reads==read_fail)return -7;
 *out=value;return 0;
}
static int adc_cali_raw_to_voltage(void*cal,unsigned,int*out) {
 ++conversions;last_calibration=cal;*out=node_mv;return convert_error;
}
namespace power_management {
struct Board {
 void *adc=reinterpret_cast<void*>(1),*calibration=reinterpret_cast<void*>(1),*external_calibration=nullptr;
 bool adc_ready=true,external_ready=false;
 int external_error=ESP_ERR_INVALID_STATE;
 void initialize_external();int external(unsigned&,unsigned&);bool battery(unsigned&,unsigned&);
};
'''
source += external + battery + r'''
static ExternalView published;
ExternalView external_view(){return published;}
}
static constexpr unsigned LV_OBJ_FLAG_HIDDEN=1;
struct Object {unsigned flags=0;};
static Object plug,battery_icon,battery_tip;
static struct {Object*plug=&::plug,*battery_icon=&::battery_icon,*battery_tip=&::battery_tip;}home;
static void lv_obj_add_flag(Object*p,unsigned f){p->flags|=f;}
static void lv_obj_clear_flag(Object*p,unsigned f){p->flags&=~f;}
static void refresh() {
''' + branch + r'''
}
int main() {
 using namespace power_management;
 Board b;unsigned raw=0,mv=0;
 b.adc=nullptr;b.initialize_external();CHECK(!b.external_ready && b.external_error==ESP_ERR_INVALID_STATE && !creates);
 b.adc=reinterpret_cast<void*>(1);config_error=-4;b.initialize_external();CHECK(b.external_error==-4 && !creates);
 config_error=0;create_error=-5;b.initialize_external();CHECK(b.external_error==-5 && !b.external_ready);
 CHECK(b.external(raw,mv)==-5 && !reads);
 create_error=0;b.initialize_external();CHECK(b.external_ready && b.external_error==0 && b.external_calibration!=b.calibration);
 CHECK(b.external(raw,mv)==0 && raw==1000 && mv==2420 && reads==4 && last_channel==0 && last_calibration==b.external_calibration);
 value=0;node_mv=0;CHECK(b.external(raw,mv)==0 && !raw && !mv);
 read_fail=reads+1;CHECK(b.external(raw,mv)==-7 && !raw && !mv);read_fail=0;
 value=4095;CHECK(b.external(raw,mv)==ESP_ERR_INVALID_RESPONSE && raw==4095);
 value=-1;CHECK(b.external(raw,mv)==ESP_ERR_INVALID_RESPONSE);
 value=1000;convert_error=-8;CHECK(b.external(raw,mv)==-8 && raw==1000 && mv==0);
 convert_error=0;node_mv=-1;CHECK(b.external(raw,mv)==ESP_ERR_INVALID_RESPONSE);
 node_mv=3101;CHECK(b.external(raw,mv)==ESP_ERR_INVALID_RESPONSE);
 node_mv=1400;CHECK(b.battery(raw,mv) && mv==4200 && last_channel==3 && last_calibration==b.calibration && b.adc_ready);
 for(auto state:{External::PRESENT,External::ABSENT,External::UNKNOWN}) {
  published.state=state;published.valid=published.calibrated=true;refresh();
  const bool plugged=state==External::PRESENT;
  CHECK(bool(plug.flags&1)==!plugged && bool(battery_icon.flags&1)==plugged && bool(battery_tip.flags&1)==plugged);
  published.valid=false;refresh();CHECK(plug.flags&1 && !battery_icon.flags && !battery_tip.flags);
 }
 published.state=External::PRESENT;published.valid=true;published.calibrated=false;refresh();CHECK(plug.flags&1 && !battery_icon.flags && !battery_tip.flags);
 published.calibrated=true;published.error=-7;refresh();CHECK(plug.flags&1 && !battery_icon.flags && !battery_tip.flags);
 std::printf("EXTERNAL_ADC_UI: PASS=2 FAIL=0 assertions=%u\n",assertions);
}
'''
compiler = Path(r"C:/msys64/ucrt64/bin/g++.exe")
env = dict(os.environ)
env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")
with tempfile.TemporaryDirectory(prefix="remote01-external-") as w:
    cpp, binary = Path(w) / "test.cpp", Path(w) / "test.exe"
    cpp.write_text(source)
    subprocess.run([str(compiler), "-std=c++23", "-O1", "-Wall", "-Wextra", "-Werror", "-fno-exceptions", "-fno-rtti", "-I", str(root / "components/power_management/include"), str(cpp), "-o", str(binary)], env=env, check=True)
    subprocess.run([str(binary)], env=env, check=True)
print("EXTERNAL_SOURCE: PASS=1 FAIL=0 assertions=12")
