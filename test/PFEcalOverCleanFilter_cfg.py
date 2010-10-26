import FWCore.ParameterSet.Config as cms  

process = cms.Process("REPROD")

# General
process.load("Configuration.StandardSequences.Reconstruction_cff")
process.load("Configuration.StandardSequences.MagneticField_AutoFromDBCurrent_cff")
process.load("Configuration.StandardSequences.GeometryExtended_cff")
process.load('Configuration/StandardSequences/FrontierConditions_GlobalTag_cff')
# Global tag for 39X
#process.GlobalTag.globaltag = 'GR_R_39X_V1::All'
# Global tag for 36X
process.GlobalTag.globaltag = 'GR_R_36X_V12::All'

# Other statements
from Configuration.GlobalRuns.reco_TLR_39X import customisePPData
customisePPData(process)

# Here include your source
process.source = cms.Source(
    "PoolSource",
    fileNames = cms.untracked.vstring(
      #'file:/tmp/pjanot/henning.root'
      #'file:reco17.root'
      )
    # eventsToProcess = cms.untracked.VEventRange('143827:62146418-143827:62146418'),
    )
process.source.secondaryFileNames = cms.untracked.vstring()
process.source.noEventSort = cms.untracked.bool(True)
process.source.duplicateCheckMode = cms.untracked.string('noDuplicateCheck')

# Number of events
process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(-1)
)

# This is for filtering events with ECAL over cleaning.
process.load('RecoParticleFlow.PFClusterProducer.PFEcalOverCleanFilter_cfi')

process.scrapping = cms.EDFilter("FilterOutScraping",
                                applyfilter = cms.untracked.bool(True),
                                debugOn = cms.untracked.bool(False),
                                numtrack = cms.untracked.uint32(10),
                                thresh = cms.untracked.double(0.25)
                                )

process.load('CommonTools.RecoAlgos.HBHENoiseFilter_cfi')

process.dump = cms.EDAnalyzer("EventContentAnalyzer")


# Local re-reco: Produce tracker rechits, pf rechits and pf clusters
# process.towerMakerPF.HcalAcceptSeverityLevel = 9
process.localReReco = cms.Sequence(process.siPixelRecHits+
                                   process.siStripMatchedRecHits+
                                   process.particleFlowCluster)

#Photon re-reco
process.photonReReco = cms.Sequence(process.conversionSequence+
                                    process.trackerOnlyConversionSequence+
                                    process.photonSequence+
                                    process.photonIDSequence)

# Track re-reco
process.globalReReco =  cms.Sequence(process.offlineBeamSpot+
                                     process.recopixelvertexing+
                                     process.ckftracks+
                                     process.ctfTracksPixelLess+
                                     process.offlinePrimaryVertices *
                                     process.offlinePrimaryVerticesWithBS *
                                     process.caloTowersRec+
                                     process.vertexreco+
                                     process.recoJets+
                                     process.muonrecoComplete+
                                     process.electronGsfTracking+
                                     process.photonReReco+
                                     process.metreco)

# Particle Flow re-processing
process.pfReReco = cms.Sequence(process.particleFlowReco+
                                process.recoPFJets+
                                process.recoPFMET+
                                process.PFTau
                                )
                                
# The complete reprocessing
process.p1 = cms.Path(process.scrapping+
                      process.HBHENoiseFilter+
                      process.pfEcalOverCleanFilter+
                      process.localReReco+
                      process.globalReReco+
                      process.pfReReco
                      # Here include your analysis sequence
                      #+process.myAnalysisSequence
                     )

process.p2 = cms.Path(process.scrapping+
                      process.HBHENoiseFilter+
                      ~process.pfEcalOverCleanFilter
                      # Here include your analysis sequence
                      #+process.myAnalysisSequence
                     )

# And the output.
process.load("Configuration.EventContent.EventContent_cff")
process.rereco = cms.OutputModule("PoolOutputModule",
    process.RECOSIMEventContent,
    fileName = cms.untracked.string('reco.root'),
    SelectEvents = cms.untracked.PSet(SelectEvents = cms.vstring('p1'))
)

# Write out only filtered and re-rereco'ed events
process.rereco.SelectEvents = cms.untracked.PSet( SelectEvents = cms.vstring('p1') )
process.outpath = cms.EndPath(process.rereco)

# Schedule the paths
process.schedule = cms.Schedule(
    process.p1,
    process.p2,
    process.outpath
)

# And the logger
process.load("FWCore.MessageLogger.MessageLogger_cfi")
process.options = cms.untracked.PSet(
    #fileMode = cms.untracked.string('NOMERGE'),
    makeTriggerResults = cms.untracked.bool(True),
    wantSummary = cms.untracked.bool(True),
    Rethrow = cms.untracked.vstring('Unknown', 
        'ProductNotFound', 
        'DictionaryNotFound', 
        'InsertFailure', 
        'Configuration', 
        'LogicError', 
        'UnimplementedFeature', 
        'InvalidReference', 
        'NullPointerError', 
        'NoProductSpecified', 
        'EventTimeout', 
        'EventCorruption', 
        'ModuleFailure', 
        'ScheduleExecutionFailure', 
        'EventProcessorFailure', 
        'FileInPathError', 
        'FatalRootError', 
        'NotFound')
)

process.MessageLogger.cerr.FwkReport.reportEvery = 1
