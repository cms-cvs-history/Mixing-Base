#include "Mixing/Base/interface/PileUp.h"
#include "FWCore/Framework/interface/EventPrincipal.h"
#include "FWCore/Framework/interface/InputSourceDescription.h"
#include "FWCore/Sources/interface/VectorInputSourceFactory.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/Exception.h"

#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Utilities/interface/RandomNumberGenerator.h"

#include <algorithm>

namespace edm {
  PileUp::PileUp(ParameterSet const& pset, double averageNumber, TH1F * const histo, const bool playback) :
    type_(pset.getParameter<std::string>("type")),
    averageNumber_(averageNumber),
    intAverage_(static_cast<int>(averageNumber)),
    histo_(histo),
    histoDistribution_(type_ == "histo"),
    probFunctionDistribution_(type_ == "probFunction"),
    poisson_(type_ == "poisson"),
    fixed_(type_ == "fixed"),
    none_(type_ == "none"),
    input_(VectorInputSourceFactory::get()->makeVectorInputSource(pset, InputSourceDescription()).release()),
    poissonDistribution_(0),
    playback_(playback),
    sequential_(pset.getUntrackedParameter<bool>("sequential", false)),
    seed_(pset.getParameter<edm::ParameterSet>("nbPileupEvents").getUntrackedParameter<int>("seed",1234))
   {

    
    edm::Service<edm::RandomNumberGenerator> rng;
    if (!rng.isAvailable()) {
      throw cms::Exception("Configuration")
	<< "PileUp requires the RandomNumberGeneratorService\n"
	"which is not present in the configuration file.  You must add the service\n"
	"in the configuration file or remove the modules that require it.";
    }
    
    CLHEP::HepRandomEngine& engine = rng->getEngine();
    poissonDistribution_ = new CLHEP::RandPoissonQ(engine, averageNumber_);
    
    // Get seed for the case when using user histogram or probability function
    if (histoDistribution_ || probFunctionDistribution_){ 
      gRandom->SetSeed(seed_);
      LogInfo("MixingModule") << " Change seed for " << type_ << " mode. The seed is set to " << seed_;
    } 
     
        
    if (!(histoDistribution_ || probFunctionDistribution_ || poisson_ || fixed_ || none_)) {
      throw cms::Exception("Illegal parameter value","PileUp::PileUp(ParameterSet const& pset)")
        << "'type' parameter (a string) has a value of '" << type_ << "'.\n"
        << "Legal values are 'poisson', 'fixed', or 'none'\n";
    }
    
  }

  PileUp::~PileUp() {
    delete poissonDistribution_;
  }


} //namespace edm
