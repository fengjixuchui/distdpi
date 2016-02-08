#include <iostream>
#include <string.h>
#include <thread>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <functional>
#include <navl.h>
#include <utility>

#include <DPIEngine.h>

void bind_navl_externals();

using namespace std::placeholders;

std::mutex dpiengine_lock;
void *ptrForCb = NULL;

namespace distdpi {

DPIEngine::DPIEngine(std::shared_ptr<FlowTable> ftbl,
                     int numthreads):
    num_of_dpi_threads(numthreads), ftbl_(ftbl) {
    ptrForCb = (void *) this;
}

int
DPIEngine::navl_classify_callback(navl_handle_t handle, navl_result_t result, navl_state_t state, navl_conn_t nc, void *arg, int error)
{
    dpiengine_lock.lock(); 
    FlowTable::ConnInfo *info = static_cast<FlowTable::ConnInfo *>(arg);
    ((DPIEngine *)ptrForCb)->ftbl_->updateFlowTableDPIData(info, handle, result, state, nc, error);
    dpiengine_lock.unlock();
    if (info->class_state == NAVL_STATE_CLASSIFIED)
        navl_conn_destroy(handle, info->dpi_state);
    return 0;
}

void DPIEngine::Dequeue(int queue) {
    bind_navl_externals();
    navl_handle_t g_navlhandle_;

    if((g_navlhandle_ = navl_open("plugins")) == 0)
        fprintf(stderr, "navl_open failed\n");

    if (navl_init(g_navlhandle_) == -1)
        fprintf(stderr, "navl_init failed\n");

    for (;;) {
        FlowTable::ConnMetadata m = ftbl_->ftbl_queue_list_[queue]->pop();
        FlowTable::ConnKey *key = m.key;
        FlowTable::ConnInfo *info = m.info;
        //std::cout << "g_navl " << g_navlhandle_ << " Src add " << key->srcaddr << " Dst addr " << key->dstaddr << " src port " <<
        //key->srcport << " dst port " << key->dstport << " ip proto " << key->ipproto << " packet num " << m.pktnum << 
        //" DPI state " << std::endl;
        if (info->dpi_state == NULL) {
            navl_host_t src_addr, dst_addr;
            src_addr.family = NAVL_AF_INET;
            src_addr.port = htons(key->srcport);
            src_addr.in4_addr = htonl(key->srcaddr);
            dst_addr.family = NAVL_AF_INET;
            dst_addr.port = htons(key->dstport);
            dst_addr.in4_addr = htonl(key->dstaddr);
            if (navl_conn_create(g_navlhandle_, &src_addr, &dst_addr, key->ipproto, &(info->dpi_state)) != 0) {
                std::cout << "Returning -----------" << std::endl; 
                continue;
            }
        }
        else {
            if (!info->error && m.data.size() && info->dpi_state && info->class_state != NAVL_STATE_CLASSIFIED) {
                if (navl_classify(g_navlhandle_, NAVL_ENCAP_NONE, m.data.c_str(), m.data.size(), info->dpi_state, m.dir, DPIEngine::navl_classify_callback, (void *)info))
                {
                    std::cout << "Returning unable to dpi :P dir " << std::endl;
                }
            }
        }
    }
    navl_fini(g_navlhandle_);
    navl_close(g_navlhandle_);
    std::cout << " Exiting thread for queue " << queue << std::endl;
}

void DPIEngine::start() {
    for (int i = 0; i < num_of_dpi_threads; i++)
        dpithreads.push_back(std::thread(&DPIEngine::Dequeue, this, i));
}

void DPIEngine::stop () {
    for (int i = 0; i < num_of_dpi_threads; i++)
        dpithreads[i].join();
    std::cout << "DPI Engine stop called " << std::endl;
}

DPIEngine::~DPIEngine() {
    std::cout << "Calling destructor" << std::endl;
}

} 
