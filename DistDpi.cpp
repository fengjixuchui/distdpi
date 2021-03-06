#include <string.h>
#include <getopt.h>
#include <iostream>
#include <syslog.h>
#include <vector>
#include <csignal>

#include <PacketHandler.h>
#include <FlowTable.h>
#include <DPIEngine.h>
#include <DistDpi.h>

namespace distdpi {

DistDpi::DistDpi() {
}

DistDpi::~DistDpi() {
}

void DistDpi::stop() {
    std::cout << "Stop called " << std::endl;

    pkt_hdl_->stop();
    ftb->stop();
    dpi_engine_->stop();
    dp_update_->stop();    
    th[0].join();
    th[1].join();
    th[2].join();
    th[3].join();
    std::unique_lock<std::mutex> lk(mutex_);
    running_ = true;
    notify = true;
    cv_.notify_one();
}

void DistDpi::start() {
    int dpi_instances = 4;

    running_ = false;
    notify = false;
    std::cout << "Starting DPI engine " << std::endl;
    signals.push_back(SIGTERM);
    signals.push_back(SIGINT);
    this->install(this, signals);
   
    dp_update_ = std::make_shared<DataPathUpdate> ();
    ftb = std::make_shared<FlowTable> (dpi_instances, dp_update_);
    dpi_engine_ = std::make_shared<DPIEngine> (ftb, dpi_instances);
    pkt_hdl_ = std::unique_ptr<PacketHandler>(new PacketHandler("serviceinstance-2", ftb));
  
    th.push_back(std::thread(&PacketHandler::start, pkt_hdl_.get()));
    th.push_back(std::thread(&DPIEngine::start, dpi_engine_.get()));
    th.push_back(std::thread(std::bind(&FlowTable::start, ftb, dpi_engine_)));
    th.push_back(std::thread(&DataPathUpdate::start, dp_update_));

    while(!running_) {
        std::unique_lock<std::mutex> lk(mutex_);
        while(!notify)
            cv_.wait(lk);
    }
    std::cout << "Exiting Distdpi " << std::endl;
}

}
