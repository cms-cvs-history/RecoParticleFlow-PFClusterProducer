#include "RecoParticleFlow/PFClusterProducer/plugins/PFEcalOverCleanFilter.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

#include "FWCore/Framework/interface/Event.h"
#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/ParticleFlowReco/interface/PFRecHitFwd.h"
#include "DataFormats/ParticleFlowReco/interface/PFRecHit.h"

using namespace reco;
using namespace edm;
using namespace std;

PFEcalOverCleanFilter::PFEcalOverCleanFilter(const edm::ParameterSet& iConfig)
{
  ecalCleanedHitLabel_         = iConfig.getParameter<edm::InputTag>("EcalCleanedHitLabel");
  energyCut_                   = iConfig.getParameter<double>("EnergyCut");
  timingCut_                   = iConfig.getParameter<double>("TimingCut");

  verbose                      = iConfig.getParameter<bool>("verbose");

}

PFEcalOverCleanFilter::~PFEcalOverCleanFilter()
{}


void 
PFEcalOverCleanFilter::beginJob() {}

void 
PFEcalOverCleanFilter::endJob() {}

bool 
PFEcalOverCleanFilter::filter(edm::Event& iEvent, const edm::EventSetup& iESetup)
{

  bool pass = false;

  // The collection of cleaned-away ECAL hits
  edm::Handle<reco::PFRecHitCollection> cleanedEcalHits;  
  bool isCleaned = iEvent.getByLabel(ecalCleanedHitLabel_, cleanedEcalHits);
				     
  if ( !isCleaned ) { 
    std::cout << "Warning : no cleaned ECAL Hits in input !" << std::endl;
    return pass;
  }

  // Loop over all cleaned-away ECAL hits
  for (unsigned int i = 0; i < cleanedEcalHits->size(); ++i) {    
    const reco::PFRecHit& hit = (*cleanedEcalHits)[i];
    double energy = hit.energy();
    double time = hit.rescale();
    if ( verbose ) std::cout << "Ecal Hit with E = " << energy
			     << " and time = " << time 
			     << " was cleaned. " << std::endl;
    if ( energy > energyCut_ && time > timingCut_ ) { 
      if ( verbose ) 
	std::cout << "This hit was over-cleaned and the event is now sent to re-reconstruction" 
		  << std::endl;
      pass = true;
      break;
    }
  }

  return pass;

}

