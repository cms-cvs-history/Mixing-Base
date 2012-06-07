// File: BMixingModule.cc
// Description:  see BMixingModule.h
// Author:  Ursula Berthon, LLR Palaiseau, Bill Tanenbaum
//
//--------------------------------------------

#include "Mixing/Base/interface/BMixingModule.h"
#include "FWCore/Utilities/interface/GetPassID.h"
#include "FWCore/Version/interface/GetReleaseVersion.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/EventPrincipal.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "DataFormats/Common/interface/Handle.h"

#include "TFile.h"
#include "TH1F.h"

using namespace std;

int edm::BMixingModule::vertexoffset = 0;
const unsigned int edm::BMixingModule::maxNbSources_ =4;

namespace
{
  boost::shared_ptr<edm::PileUp>
  maybeMakePileUp(edm::ParameterSet const& ps,std::string sourceName, const int minb, const int maxb, const bool playback)
  { 
    boost::shared_ptr<edm::PileUp> pileup; // value to be returned
    // Make sure we have a parameter named 'sourceName'
    if (ps.exists(sourceName))
      {
	// We have the parameter
	// and if we have either averageNumber or cfg by luminosity... make the PileUp
	double averageNumber;
        std::string histoFileName=" ";
	std::string histoName =" ";
	TH1F * h = new TH1F("h","h",10,0,10);
	vector<int> dataProbFunctionVar;
	vector<double> dataProb;
	
	
	const edm::ParameterSet & psin=ps.getParameter<edm::ParameterSet>(sourceName);
	std::string type_=psin.getParameter<std::string>("type");
	if (ps.exists("readDB") && ps.getParameter<bool>("readDB")){
	  //in case of DB access, do not try to load anything from the PSet, but wait for beginRun.
 	  edm::LogError("BMixingModule")<<"Will read from DB: reset to a dummy PileUp object.";
	  pileup.reset(new edm::PileUp(psin,0,0,playback));
	  return pileup;
	}
	if (type_!="none"){
	  if (psin.exists("nbPileupEvents")) {
	    edm::ParameterSet psin_average=psin.getParameter<edm::ParameterSet>("nbPileupEvents");
	    if (psin_average.exists("averageNumber"))
	      {
		averageNumber=psin_average.getParameter<double>("averageNumber");
		pileup.reset(new edm::PileUp(psin,averageNumber,h,playback));
		edm::LogInfo("MixingModule") <<" Created source "<<sourceName<<" with averageNumber "<<averageNumber;
	      }
	    else if (psin_average.exists("fileName") && psin_average.exists("histoName")) {
		std::string histoFileName = psin_average.getUntrackedParameter<std::string>("fileName");
		std::string histoName = psin_average.getUntrackedParameter<std::string>("histoName");
						
		TFile *infile = new TFile(histoFileName.c_str());
   	 	TH1F *h = (TH1F*)infile->Get(histoName.c_str());
                
		// Check if the histogram exists           
      		if (!h) {
        	  throw cms::Exception("HistogramNotFound") << " Could not find the histogram " << histoName 
      		  					    << "in the file " << histoFileName << "." << std::endl;
      		}else{
		  edm::LogInfo("MixingModule") << "Open a root file " << histoFileName << " containing the probability distribution histogram " << histoName << std::endl;
		  edm::LogInfo("MixingModule") << "The PileUp number to be added will be chosen randomly from this histogram" << std::endl;
		}
                
		// Check if the histogram is normalized
		if (((h->Integral() - 1) > 1.0e-02) && ((h->Integral() - 1) < -1.0e-02)) throw cms::Exception("BadHistoDistribution") << "The histogram should be normalized!" << std::endl;
						
		// Get the averageNumber from the histo 
		averageNumber = h->GetMean();
		
		pileup.reset(new edm::PileUp(psin,averageNumber,h,playback));
		edm::LogInfo("MixingModule") <<" Created source "<<sourceName<<" with averageNumber "<<averageNumber;
	      
	      }
	    else if (psin_average.exists("probFunctionVariable") && psin_average.exists("probValue") && psin_average.exists("histoFileName"))
	      {
		if (type_!="probFunction") {
		  edm::LogError("MisConfiguration")<<"type is set to: "<<type_<<" while parameters implies probFunction; changing.";
		  type_="probFunction";
		}

	        dataProbFunctionVar = psin_average.getParameter<vector<int> >("probFunctionVariable");
  		dataProb = psin_average.getParameter<vector<double> >("probValue");
	        histoFileName = psin_average.getUntrackedParameter<std::string>("histoFileName"); 
							
		int varSize = (int) dataProbFunctionVar.size();
		int probSize = (int) dataProb.size();
		
		if ((dataProbFunctionVar[0] != 0) || (dataProbFunctionVar[varSize - 1] != (varSize - 1))) 
		  throw cms::Exception("BadProbFunction") << "Please, check the variables of the probability function! The first variable should be 0 and the difference between two variables should be 1." << endl;
		
		// Complete the vector containing the probability  function data
		// with the values "0"
		if (probSize < varSize){
		  edm::LogInfo("MixingModule") << " The probability function data will be completed with " <<(varSize - probSize)  <<" values 0."; 
		  
		  for (int i=0; i<(varSize - probSize); i++) dataProb.push_back(0);
		  
		  probSize = dataProb.size();
		  edm::LogInfo("MixingModule") << " The number of the P(x) data set after adding the values 0 is " << probSize;
		}
			 		
		// Create an histogram with the data from the probability function provided by the user		  
		int xmin = (int) dataProbFunctionVar[0];
		int xmax = (int) dataProbFunctionVar[varSize-1]+1;  // need upper edge to be one beyond last value
		int numBins = varSize;
		
		edm::LogInfo("MixingModule") << "An histogram will be created with " << numBins << " bins in the range ("<< xmin << "," << xmax << ")." << std::endl;
				
		TH1F *hprob = new TH1F("h","Histo from the user's probability function",numBins,xmin,xmax); 
		
		LogDebug("MixingModule") << "Filling histogram with the following data:" << std::endl;
		
		for (int j=0; j < numBins ; j++){
		  LogDebug("MixingModule") << " x = " << dataProbFunctionVar[j ]<< " P(x) = " << dataProb[j];
		  hprob->Fill(dataProbFunctionVar[j]+0.5,dataProb[j]); // assuming integer values for the bins, fill bin centers, not edges 
	   	}
				
		// Check if the histogram is normalized
		if ( ((hprob->Integral() - 1) > 1.0e-02) && ((hprob->Integral() - 1) < -1.0e-02)){ 
		  throw cms::Exception("BadProbFunction") << "The probability function should be normalized!!! " << endl;
		}
		
		averageNumber = hprob->GetMean();
				
		// Write the created histogram into a root file
		edm::LogInfo("MixingModule") << " The histogram created from the x, P(x) values will be written into the root file " << histoFileName; 
		
		TFile * outfile = new TFile(histoFileName.c_str(),"RECREATE");
		hprob->Write();
		outfile->Write();
		outfile->Close();
		outfile->Delete();		
		
		pileup.reset(new edm::PileUp(psin,averageNumber,hprob,playback));
		edm::LogInfo("MixingModule") <<" Created source "<<sourceName<<" with averageNumber "<<averageNumber;
		
	      } 
	    //special for pileup input
	    else if (sourceName=="input" && psin_average.exists("Lumi") && psin_average.exists("sigmaInel")) {
	      averageNumber=psin_average.getParameter<double>("Lumi")*psin_average.getParameter<double>("sigmaInel")*ps.getParameter<int>("bunchspace")/1000*3564./2808.;  //FIXME
	      pileup.reset(new
 	      edm::PileUp(psin,averageNumber,h,playback));
	      edm::LogInfo("MixingModule") <<" Created source "<<sourceName<<" with minBunch,maxBunch "<<minb<<" "<<maxb;
	      edm::LogInfo("MixingModule")<<" Luminosity configuration, average number used is "<<averageNumber;
	    }
	  }
	}
      }
    return pileup;
  }
}

namespace edm {

  // Constructor 
  BMixingModule::BMixingModule(const edm::ParameterSet& pset) :
    bunchSpace_(pset.getParameter<int>("bunchspace")),
    minBunch_((pset.getParameter<int>("minBunch")*25)/pset.getParameter<int>("bunchspace")),
    maxBunch_((pset.getParameter<int>("maxBunch")*25)/pset.getParameter<int>("bunchspace")),
    mixProdStep1_(pset.getParameter<bool>("mixProdStep1")),
    mixProdStep2_(pset.getParameter<bool>("mixProdStep2")),
    readDB_(false)
  {
    if (pset.exists("readDB"))      readDB_=pset.getParameter<bool>("readDB");

    playback_=pset.getUntrackedParameter<bool>("playback",false);

    if (playback_) {
      //this could be explicitely checked
      LogInfo("MixingModule") <<" ATTENTION:Mixing will be done in playback mode! \n"
                              <<" ATTENTION:Mixing Configuration must be the same as for the original mixing!";
    }

    // Just for debugging print out.
    sourceNames_.push_back("input");
    sourceNames_.push_back("cosmics");
    sourceNames_.push_back("beamhalo_plus");
    sourceNames_.push_back("beamhalo_minus");

    for (size_t makeIdx = 0; makeIdx < maxNbSources_; makeIdx++ ) {
      inputSources_.push_back(maybeMakePileUp(pset,sourceNames_[makeIdx],
                                              minBunch_,maxBunch_,playback_));
      if (inputSources_.back()) inputSources_.back()->input(makeIdx);
    }
  }

  // Virtual destructor needed.
  BMixingModule::~BMixingModule() {;}

  // method call at begin run/lumi to reload the mixing configuration
  void BMixingModule::beginLuminosityBlock(edm::LuminosityBlock&, edm::EventSetup const&setup){
    update(setup);
  }

  void BMixingModule::beginRun(edm::Run & r, const edm::EventSetup & setup){
    update(setup);
  }

  void BMixingModule::update(const edm::EventSetup & setup){
    if (readDB_ && parameterWatcher_.check(setup)){
      for (size_t makeIdx = 0; makeIdx < maxNbSources_; makeIdx++ ) {
	if (inputSources_[makeIdx]) inputSources_[makeIdx]->reload(setup);
      }
      reload(setup);
    }
  }

  // Functions that get called by framework every event
  void BMixingModule::produce(edm::Event& e, const edm::EventSetup& setup) { 

    // Check if the signal is present in the root file 
    // for all the objects we want to mix
    checkSignal(e);
    
    // Create EDProduct
    createnewEDProduct();

    initializeEvent(e, setup);

    // Add signals
    if (!mixProdStep1_){ 
      addSignals(e,setup);
    }

    doPileUp(e,setup);

    // Includes putting digi products into the edm::Event.
    finalizeEvent(e, setup);

    // Put output into event (here only playback info)
    put(e,setup);
  }

  void BMixingModule::dropUnwantedBranches(std::vector<std::string> const& wantedBranches) {
    for (size_t dropIdx=0; dropIdx<maxNbSources_; dropIdx++ ) {
      if( inputSources_[dropIdx] ) inputSources_[dropIdx]->dropUnwantedBranches(wantedBranches);
    }
  }

  void BMixingModule::endJob() {
    for (size_t endIdx=0; endIdx<maxNbSources_; endIdx++ ) {
      if( inputSources_[endIdx] ) inputSources_[endIdx]->endJob();
    }
  }

} //edm
