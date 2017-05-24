
// -- std headers
#include <iostream>
#include <ctime>

// -- lcio headers
#include "EVENT/LCIO.h"
#include "IO/LCReader.h"
#include "IOIMPL/LCFactory.h"

// -- dqm4hep
#include "dqm4hep/DQM4HEP.h"
#include "dqm4hep/DQMLogging.h"
#include "dqm4hep/DQMPluginManager.h"
#include "dqm4hep/DQMDimEventClient.h"

// -- dqm4ilc headers
#include "dqm4ilc/DQMLcioReaderListener.h"
#include "dqm4ilc/DQMLCEventStreamer.h"

// -- tclap headers
#include "tclap/CmdLine.h"
#include "tclap/Arg.h"

// -- dim headers
#include "dis.hxx"

// -- xdrstream headers
#include "xdrstream/BufferDevice.h"

// -- xdrlcio headers
#include "xdrlcio/XdrLcio.h"

// -- lcio headers
#include "IMPL/CalorimeterHitImpl.h"
#include "IMPL/LCCollectionVec.h"
#include "IMPL/LCEventImpl.h"
#include "IMPL/LCFlagImpl.h"
#include "UTIL/LCTOOLS.h"

using namespace std;
using namespace dqm4hep;
using namespace dqm4ilc;

#define CH_TEST_RETURN( Command )					\
  {									\
    xdrstream::Status status = Command;					\
    if( status != xdrstream::XDR_SUCCESS )				\
      {									\
	std::cout << "Error caught line " << __LINE__ << " of command " << #Command << std::endl; \
	xdrstream::printStatus( status );				\
	return status;							\
      }									\
  }

int main(int argc, char* argv[])
{
  DQM4HEP::screenSplash();

  std::string cmdLineFooter = "Please report bug to <rete@ipnl.in2p3.fr>";
  TCLAP::CmdLine *pCommandLine = new TCLAP::CmdLine(cmdLineFooter, ' ', DQM4HEP_VERSION_STR);
  std::string log4cxx_file = std::string(DQMCore_DIR) + "/conf/defaultLoggerConfig.xml";

  TCLAP::ValueArg<unsigned int> sleepTimeArg(
					     "t"
					     , "sleep-time"
					     , "The sleep time between each event (unit msec)"
					     , false
					     , 1000
					     , "unsigned int");
  pCommandLine->add(sleepTimeArg);

  TCLAP::ValueArg<unsigned int> skipNEventsArg(
					       "n"
					       , "skip-event"
					       , "The number of events to skip form file beginning"
					       , false
					       , 0
					       , "unsigned int");
  pCommandLine->add(skipNEventsArg);

  // Section for lcio filenames commented out -- will be replaced with commandline argument parsing for event stream
  // Should probably hardcode this to begin with, then worry about parsing later

  /*
    TCLAP::ValueArg<std::string> lcioFileNamesArg(
    "f"
    , "lcio-files"
    , "The list of lcio files to process (separated by a ':' character)"
    , true
    , ""
    , "string");
    pCommandLine->add(lcioFileNamesArg);
  */

  TCLAP::ValueArg<std::string> eventCollectorNameArg(
						     "c"
						     , "collector-name"
						     , "The event collector name in which the event will be published"
						     , true
						     , ""
						     , "string");
  pCommandLine->add(eventCollectorNameArg);

  TCLAP::ValueArg<std::string> loggerConfigArg(
					       "l"
					       , "logger-config"
					       , "The xml logger file to configure log4cxx"
					       , false
					       , log4cxx_file
					       , "string");
  pCommandLine->add(loggerConfigArg);

  std::vector<std::string> allowedLevels;
  allowedLevels.push_back("INFO");
  allowedLevels.push_back("WARN");
  allowedLevels.push_back("DEBUG");
  allowedLevels.push_back("TRACE");
  allowedLevels.push_back("ERROR");
  allowedLevels.push_back("FATAL");
  allowedLevels.push_back("OFF");
  allowedLevels.push_back("ALL");
  TCLAP::ValuesConstraint<std::string> allowedLevelsContraint( allowedLevels );

  TCLAP::ValueArg<std::string> verbosityArg(
					    "v"
					    , "verbosity"
					    , "The verbosity level used for this application"
					    , false
					    , "INFO"
					    , &allowedLevelsContraint);
  pCommandLine->add(verbosityArg);

  TCLAP::SwitchArg simulateSpillArg(
				    "s"
				    , "simulate-spill"
				    , "Whether a spill structure has to be simulated using getTimeStamp() of LCEvents"
				    , false);
  pCommandLine->add(simulateSpillArg);

  // parse command line
  pCommandLine->parse(argc, argv);

  log4cxx_file = loggerConfigArg.getValue();
  log4cxx::xml::DOMConfigurator::configure(log4cxx_file);

  if( verbosityArg.isSet() )
    dqmMainLogger->setLevel( log4cxx::Level::toLevel( verbosityArg.getValue() ) );

  // Arranging input files
  /*std::vector<std::string> lcioInputFiles;
    DQM4HEP::tokenize(lcioFileNamesArg.getValue(), lcioInputFiles, ":");*/

  // New XDRLCIO stuff
  xdrlcio::XdrLcio *pXdrLcio = new xdrlcio::XdrLcio();
  xdrstream::BufferDevice *pInDevice = new xdrstream::BufferDevice(pOutDevice->getBuffer() , pOutDevice->getPosition(), false);
  pInDevice->setOwner(false);
  // ==    ==    == //

  // file reader
  IO::LCReader *pLCReader = IOIMPL::LCFactory::getInstance()->createLCReader(0);

  // event collector client
  DQMEventClient *pEventClient = new DQMDimEventClient();

  DQMLCEventStreamer *pEventStreamer = new DQMLCEventStreamer();
  pEventClient->setEventStreamer(pEventStreamer);

  pEventClient->setCollectorName(eventCollectorNameArg.getValue());

  // lcio file listener
  DQMLcioReaderListener *pListener = new DQMLcioReaderListener(pLCReader);
  pListener->setSimulateSpill(simulateSpillArg.getValue());
  pListener->setEventClient(pEventClient);
  pListener->setSleepTime(sleepTimeArg.getValue());

  while(1)
    {
      try
	{

	  // This is the actual meat; here is where we will pull events from the in-stream. We have to figure out how to send this event we're read in to the event collector service.

	  // New XDRLCIO stuff
	  CH_TEST_RETURN( pXdrLcio->readNextEvent( pInDevice ) );
	  EVENT::LCEvent *pReadLCEvent = pXdrLcio->getLCEvent();
	  pXdrLcio->readStream();
	  // ==    ==    == //

	  // Old static file stuff: 
	  /*pLCReader->open(lcioInputFiles);

	    if( skipNEventsArg.getValue() )
	    pLCReader->skipNEvents( skipNEventsArg.getValue() );

	    pLCReader->readStream();
	    pLCReader->close();
	  */

	}
      catch(StatusCodeException &exception)
	{
	  LOG4CXX_ERROR( dqmMainLogger , "StatusCodeException caught while reading stream : " << exception.toString() );
	  break;
	}
      catch(std::exception & exception)
	{
	  LOG4CXX_ERROR( dqmMainLogger , "std::exception caught while reading stream : " << exception.what() );
	  break;
	}

      LOG4CXX_INFO( dqmMainLogger , "Exiting lcio file service ..." );

    }

  delete pEventClient;
  delete pListener;
  delete pLCReader;
  delete pCommandLine;

  return 0;
}
