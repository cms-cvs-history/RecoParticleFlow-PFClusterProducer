import FWCore.ParameterSet.Config as cms  

process = cms.Process("RECO")

# General
process.load("Configuration.StandardSequences.RawToDigi_Data_cff")
process.load("Configuration.StandardSequences.Reconstruction_cff")
process.load("Configuration.StandardSequences.MagneticField_AutoFromDBCurrent_cff")
process.load("Configuration.StandardSequences.GeometryExtended_cff")
process.load('Configuration/StandardSequences/FrontierConditions_GlobalTag_cff')
# Global tag for 39X
process.GlobalTag.globaltag = 'GR_R_39X_V1::All'
# Global tag for 36X
#process.GlobalTag.globaltag = 'GR_R_36X_V12::All'

# Other statements
from Configuration.GlobalRuns.reco_TLR_39X import customisePPData
customisePPData(process)

# Here include your source
process.source = cms.Source(
    "PoolSource",
    fileNames = cms.untracked.vstring(
      #'file:/tmp/pjanot/henning.root'
      'file:/tmp/pjanot/17_events_raw.root'
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

# The complete reprocessing
process.p1 = cms.Path(process.RawToDigi+
                      process.reconstruction
                      # Here include your analysis sequence
                      #+process.myAnalysisSequence
                     )

# And the output.
process.load("Configuration.EventContent.EventContent_cff")
process.rereco = cms.OutputModule("PoolOutputModule",
    process.RECOSIMEventContent,
    fileName = cms.untracked.string('reco17.root'),
    SelectEvents = cms.untracked.PSet(SelectEvents = cms.vstring('p'))
)

# Write out only filtered and re-rereco'ed events
process.rereco.SelectEvents = cms.untracked.PSet( SelectEvents = cms.vstring('p1') )
process.outpath = cms.EndPath(process.rereco)

# Schedule the paths
process.schedule = cms.Schedule(
    process.p1,
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
