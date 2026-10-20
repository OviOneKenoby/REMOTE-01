#include "rtcp_client/mdns_discovery.hpp"

#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

namespace rtcp_client {
namespace {
constexpr std::size_t stack_words=discovery_worker_stack_bytes/sizeof(StackType_t);
StaticTask_t task_storage{};
StackType_t task_stack[stack_words]{};
TaskHandle_t task_handle=nullptr;
portMUX_TYPE guard=portMUX_INITIALIZER_UNLOCKED;
DiscoveryBatch published{};
bool initialized=false,network_available=false,busy=false,ready=false;

void publish(DiscoveryBatch batch){
  taskENTER_CRITICAL(&guard);
  if(network_available){published=batch;ready=true;}
  busy=false;
  taskEXIT_CRITICAL(&guard);
}

void query_task(void*){
  for(;;){
    ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    taskENTER_CRITICAL(&guard);const bool run=network_available;taskEXIT_CRITICAL(&guard);
    if(!run){publish({});continue;}
    mdns_result_t*results=nullptr;
    DiscoveryBatch batch{};
    batch.error=mdns_query_ptr("_rtcp","_tcp",discovery_query_timeout_ms,
                               discovery_candidate_limit,&results);
    if(batch.error==ESP_OK){
      for(auto*r=results;r&&batch.count<batch.endpoints.size();r=r->next){
        if(r->port!=46411||!r->addr||!r->ttl)continue;
        for(auto*a=r->addr;a&&batch.count<batch.endpoints.size();a=a->next){
          if(a->addr.type!=ESP_IPADDR_TYPE_V4)continue;
          Endpoint candidate{};candidate.port=r->port;
          std::memcpy(candidate.ipv4.data(),&a->addr.u_addr.ip4.addr,4);
          bool duplicate=false;for(std::uint8_t i=0;i<batch.count;++i)duplicate|=batch.endpoints[i]==candidate;
          if(!duplicate)batch.endpoints[batch.count++]=candidate;
        }
      }
    }
    if(results)mdns_query_results_free(results);
    publish(batch);
  }
}
}

bool discovery_initialize(){
  if(initialized)return true;
  if(mdns_init()!=ESP_OK)return false;
  task_handle=xTaskCreateStatic(query_task,"rtcp-mdns",stack_words,nullptr,1,
                                task_stack,&task_storage);
  initialized=task_handle!=nullptr;
  if(!initialized)mdns_free();
  return initialized;
}

void discovery_set_network(bool available){
  taskENTER_CRITICAL(&guard);
  network_available=available;
  if(!available){published={};ready=false;}
  taskEXIT_CRITICAL(&guard);
}

bool discovery_request(){
  taskENTER_CRITICAL(&guard);
  const bool start=initialized&&network_available&&!busy;
  if(start)busy=true;
  taskEXIT_CRITICAL(&guard);
  if(start)xTaskNotifyGive(task_handle);
  return start;
}

bool discovery_take(DiscoveryBatch&out){
  taskENTER_CRITICAL(&guard);
  const bool have=ready;
  if(have){out=published;ready=false;}
  taskEXIT_CRITICAL(&guard);
  return have;
}

} // namespace rtcp_client
