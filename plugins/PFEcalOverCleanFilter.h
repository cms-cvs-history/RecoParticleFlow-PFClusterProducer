#ifndef PFECALOVERCLEANFILTER_H
#define PFECALOVERCLEANFILTER_H

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/Framework/interface/EDFilter.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

class PFEcalOverCleanFilter: public edm::EDFilter {
 public:

  explicit PFEcalOverCleanFilter(const edm::ParameterSet&);
  ~PFEcalOverCleanFilter();

 private:

  virtual void beginJob() ;
  virtual bool filter(edm::Event&, const edm::EventSetup&);
  virtual void endJob();

  double resolution(double, bool);
  double response(double, bool);

  double energyCut_;
  double timingCut_;
  edm::InputTag ecalCleanedHitLabel_;
  bool verbose;
  
};


#endif //PFECALOVERCLEANFILTER_H
