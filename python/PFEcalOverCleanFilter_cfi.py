import FWCore.ParameterSet.Config as cms

pfEcalOverCleanFilter = cms.EDFilter("PFEcalOverCleanFilter",
    # Cleaned-away Ecal Hit
    EcalCleanedHitLabel = cms.InputTag('particleFlowRecHitECAL:Cleaned'),
    # Energy cut for the cleaned hit
    EnergyCut = cms.double(130.0),
    # Timing cut for the cleaned hit
    TimingCut = cms.double(0.0),
    # debug level
    verbose = cms.bool(True)
                           
)
