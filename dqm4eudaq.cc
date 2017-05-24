#include "eudaq/DataCollector.hh"
#include "eudaq/TransportServer.hh"
#include "eudaq/BufferSerializer.hh"
#include "eudaq/Logger.hh"
#include "eudaq/Utils.hh"
#include <iostream>
#include <ostream>
#include <ctime>
#include <iomanip>

#include <mutex>
#include <deque>
#include <map>
#include <set>

 namespace eudaq {

   class DQMDataCollector:public eudaq::DQMDataCollector {

   public:
     DataCollector(const std::string &name, const std::string &runcontrol);

     // Below, we should only mention the ones we actually need to override.
     // What do we need to override? We basically need to add features that 1) open/create the xdrlcio device, and 2) look at the incoming data and send it to the xdrlcio device
     // We definitely need to override OnConfigure to open/create the xdrlcio interface.
     // We also need to intialise something -- whatever method we use to define the name/identifier/port of the xdrlcio interface, this needs to be read from a steering file.


     void OnInitialise() override final;
     void OnConfigure() override final;
     void OnStartRun() override final;
     void OnStopRun() override final;
     void OnTerminate() override final;
     void OnStatus() override final;

     //running in commandreceiver thread
     virtual void DoInitialise(){
       auto ini = GetInitConfiguration();
       std::ofstream ofile;
       std::string stream_target;
       stream_target = ini->Get("STREAM_TARGET", stream_target);
       m_backup_save_file_path = ini->Get("BACKUP_SAVE_FILE_PATH", "ex0dummy.txt");
       ofile.open(m_backup_save_file_path);
       if(!ofile.is_open()){
	 EUDAQ_THROW("backup save file (" + m_backup_save_file_path +") can not open for writing");
       }
       ofile << stream_target;
       // We might do other stuff here; the output file is now open, the string that contains the stream target has now been written to it, and it's about to be closed.
       ofile.close();
     };

     virtual void DoConfigure(){};
     virtual void DoStartRun(){
       xdrstream::BufferDevice *pOutDevice = new xdrstream::BufferDevice(1024*1024);
       pOutDevice->setOwner(false);
     };
     virtual void DoStopRun(){
       delete pOutDevice;
     };
     virtual void DoTerminate(){};

     //running in dataserver thread
     virtual void DoConnect(ConnectionSPC id) {
       std::unique_lock<std::mutex> lk(m_mtx_map);
       m_conn_evque[idx].clear();
       m_conn_inactive.erase(idx);
     }

     virtual void DoDisconnect(ConnectionSPC id) {
       std::unique_lock<std::mutex> lk(m_mtx_map);
       m_conn_inactive.insert(idx);
     }

     virtual void DoReceive(ConnectionSPC id, EventUP ev){
 
       std::unique_lock<std::mutex> lk(m_mtx_map);
       eudaq::EventSP evsp = std::move(ev);
       if(!evsp->IsFlagTrigger()){
	 EUDAQ_THROW("!evsp->IsFlagTrigger()");
       }
       m_conn_evque[idx].push_back(evsp);

       // More stuff? Stuff below hasn't been renamed for this file, and also I don't know what it does

       auto ev_sync = eudaq::Event::MakeUnique("Ex0Tg");
       ev_sync->SetFlagPacket();
       ev_sync->SetTriggerN(trigger_n);
       for(auto &conn_evque: m_conn_evque){
	 auto &ev_front = conn_evque.second.front(); 
	 if(ev_front->GetTriggerN() == trigger_n){
	   ev_sync->AddSubEvent(ev_front);
	   conn_evque.second.pop_front();
	 }
       }
  
       if(!m_conn_inactive.empty()){
	 std::set<eudaq::ConnectionSPC> conn_inactive_empty;
	 for(auto &conn: m_conn_inactive){
	   if(m_conn_evque.find(conn) != m_conn_evque.end() && 
	      m_conn_evque[conn].empty()){
	     m_conn_evque.erase(conn);
	     conn_inactive_empty.insert(conn);	
	   }
	 }
	 for(auto &conn: conn_inactive_empty){
	   m_conn_inactive.erase(conn);
	 }
       }
       ev_sync->Print(std::cout);
       WriteEvent(std::move(ev_sync));
     };

     void WriteEvent(EventUP ev);
     void SetServerAddress(const std::string &addr){m_data_addr = addr;};
     void StartDataCollector();
     void CloseDataCollector();
     bool IsActiveDataCollector(){return m_thd_server.joinable();}

   private:
     void DataHandler(TransportEvent &ev);
     void DataThread();

   private:
     std::thread m_thd_server;
     bool m_exit;
     std::unique_ptr<TransportServer> m_dataserver;
     std::string m_data_addr;
     FileWriterUP m_writer;
     std::string m_fwpatt;
     std::string m_fwtype;
     std::vector<std::shared_ptr<ConnectionInfo>> m_info_pdc;
     uint32_t m_dct_n;
     uint32_t m_evt_c;
     std::unique_ptr<const Configuration> m_conf;
   };

 }
