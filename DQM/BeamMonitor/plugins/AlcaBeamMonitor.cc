/*
 * \file AlcaBeamMonitor.cc
 * \author Lorenzo Uplegger/FNAL
 * modified by Simone Gennai INFN/Bicocca
 */

#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "DataFormats/BeamSpot/interface/BeamSpot.h"
//#include "DataFormats/Scalers/interface/BeamSpotOnline.h"
#include "DQM/BeamMonitor/plugins/AlcaBeamMonitor.h"
#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/Common/interface/View.h"
#include "FWCore/Framework/interface/ConsumesCollector.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/LuminosityBlock.h"
#include "FWCore/Framework/interface/Run.h"
#include "RecoVertex/BeamSpotProducer/interface/BeamFitter.h"
#include "RecoVertex/BeamSpotProducer/interface/PVFitter.h"
#include <memory>

#include <numeric>

using namespace std;
using namespace edm;
using namespace reco;

//----------------------------------------------------------------------------------------------------------------------
AlcaBeamMonitor::AlcaBeamMonitor(const ParameterSet& ps)
    : monitorName_(ps.getUntrackedParameter<string>("MonitorName")),
      primaryVertexLabel_(consumes<VertexCollection>(ps.getUntrackedParameter<InputTag>("PrimaryVertexLabel"))),
      trackLabel_(consumes<reco::TrackCollection>(ps.getUntrackedParameter<InputTag>("TrackLabel"))),
      scalerLabel_(consumes<BeamSpot>(ps.getUntrackedParameter<InputTag>("ScalerLabel"))),
      beamSpotToken_(esConsumes<edm::Transition::BeginLuminosityBlock>()),
      perLSsaving_(ps.getUntrackedParameter<bool>("perLSsaving", false)),
      numberOfValuesToSave_(0) {
  if (!monitorName_.empty())
    monitorName_ = monitorName_ + "/";

  theBeamFitter_ = std::make_unique<BeamFitter>(ps, consumesCollector());
  theBeamFitter_->resetTrkVector();
  theBeamFitter_->resetLSRange();
  theBeamFitter_->resetRefTime();
  theBeamFitter_->resetPVFitter();

  thePVFitter_ = std::make_unique<PVFitter>(ps, consumesCollector());

  processedLumis_.clear();

  varNamesV_.push_back("x");
  varNamesV_.push_back("y");
  varNamesV_.push_back("z");
  varNamesV_.push_back("sigmaX");
  varNamesV_.push_back("sigmaY");
  varNamesV_.push_back("sigmaZ");

  if (!perLSsaving_) {
    histoByCategoryNames_.insert(pair<string, string>("run", "Coordinate"));
    histoByCategoryNames_.insert(pair<string, string>("run", "PrimaryVertex fit-DataBase"));
    histoByCategoryNames_.insert(pair<string, string>("run", "PrimaryVertex fit-BeamFit"));
    histoByCategoryNames_.insert(pair<string, string>("run", "PrimaryVertex fit-Online"));
    histoByCategoryNames_.insert(pair<string, string>("run", "PrimaryVertex-DataBase"));
    histoByCategoryNames_.insert(pair<string, string>("run", "PrimaryVertex-BeamFit"));
    histoByCategoryNames_.insert(pair<string, string>("run", "PrimaryVertex-Online"));

    histoByCategoryNames_.insert(pair<string, string>("lumi", "Lumibased BeamSpotFit"));
    histoByCategoryNames_.insert(pair<string, string>("lumi", "Lumibased PrimaryVertex"));
    histoByCategoryNames_.insert(pair<string, string>("lumi", "Lumibased DataBase"));
    histoByCategoryNames_.insert(pair<string, string>("lumi", "Lumibased Online"));
    histoByCategoryNames_.insert(pair<string, string>("lumi", "Lumibased PrimaryVertex-DataBase fit"));
    histoByCategoryNames_.insert(pair<string, string>("lumi", "Lumibased PrimaryVertex-Online fit"));
    histoByCategoryNames_.insert(pair<string, string>("validation", "Lumibased Online-DataBase fit"));
    histoByCategoryNames_.insert(pair<string, string>("validation", "Lumibased PrimaryVertex-DataBase"));
    histoByCategoryNames_.insert(pair<string, string>("validation", "Lumibased PrimaryVertex-Online"));
  }

  for (vector<string>::iterator itV = varNamesV_.begin(); itV != varNamesV_.end(); itV++) {
    for (multimap<string, string>::iterator itM = histoByCategoryNames_.begin(); itM != histoByCategoryNames_.end();
         itM++) {
      histosMap_[*itV][itM->first][itM->second] = nullptr;
    }
  }
}

void AlcaBeamMonitor::fillDescriptions(edm::ConfigurationDescriptions& iDesc) {
  edm::ParameterSetDescription ps;

  ps.addUntracked<std::string>("MonitorName", "YourSubsystemName");
  ps.addUntracked<edm::InputTag>("PrimaryVertexLabel");
  ps.addUntracked<edm::InputTag>("TrackLabel");
  ps.addUntracked<edm::InputTag>("ScalerLabel");
  ps.addUntracked<bool>("perLSsaving");

  BeamFitter::fillDescription(ps);
  PVFitter::fillDescription(ps);

  iDesc.addDefault(ps);
}

//----------------------------------------------------------------------------------------------------------------------
void AlcaBeamMonitor::bookHistograms(DQMStore::IBooker& ibooker, edm::Run const& iRun, edm::EventSetup const& iSetup) {
  string name;
  string title;
  int firstLumi = 1;
  int lastLumi = 3000;
  for (HistosContainer::iterator itM = histosMap_.begin(); itM != histosMap_.end(); itM++) {
    ibooker.setCurrentFolder(monitorName_ + "Debug");
    for (map<string, MonitorElement*>::iterator itMM = itM->second["run"].begin(); itMM != itM->second["run"].end();
         itMM++) {
      name = string("h") + itM->first + itMM->first;
      title = itM->first + "_{0} " + itMM->first;
      if (itM->first == "x" || itM->first == "y") {
        if (itMM->first == "Coordinate") {
          itMM->second = ibooker.book1D(name, title, 1001, -0.2525, 0.2525);
        } else if (itMM->first == "PrimaryVertex fit-DataBase" || itMM->first == "PrimaryVertex fit-BeamFit" ||
                   itMM->first == "PrimaryVertex fit-Online" || itMM->first == "PrimaryVertex-DataBase" ||
                   itMM->first == "PrimaryVertex-BeamFit" || itMM->first == "PrimaryVertex-Online") {
          itMM->second = ibooker.book1D(name, title, 1001, -0.02525, 0.02525);
        } else {
          //assert(0);
        }
      } else if (itM->first == "z") {
        if (itMM->first == "Coordinate") {
          itMM->second = ibooker.book1D(name, title, 101, -5.05, 5.05);
        } else if (itMM->first == "PrimaryVertex fit-DataBase" || itMM->first == "PrimaryVertex fit-BeamFit" ||
                   itMM->first == "PrimaryVertex fit-Online") {
          itMM->second = ibooker.book1D(name, title, 101, -0.505, 0.505);
        } else if (itMM->first == "PrimaryVertex-DataBase" || itMM->first == "PrimaryVertex-BeamFit" ||
                   itMM->first == "PrimaryVertex-Online") {
          itMM->second = ibooker.book1D(name, title, 1001, -5.005, 5.005);
        } else {
          //assert(0);
        }
      } else if (itM->first == "sigmaX" || itM->first == "sigmaY") {
        if (itMM->first == "Coordinate") {
          itMM->second = ibooker.book1D(name, title, 100, 0, 0.015);
        } else if (itMM->first == "PrimaryVertex fit-DataBase" || itMM->first == "PrimaryVertex fit-BeamFit" ||
                   itMM->first == "PrimaryVertex fit-Online" || itMM->first == "PrimaryVertex-DataBase" ||
                   itMM->first == "PrimaryVertex-BeamFit" || itMM->first == "PrimaryVertex-Online") {
          itMM->second = nullptr;
        } else {
          //assert(0);
        }
      } else if (itM->first == "sigmaZ") {
        if (itMM->first == "Coordinate") {
          itMM->second = ibooker.book1D(name, title, 110, 0, 11);
        } else if (itMM->first == "PrimaryVertex fit-DataBase" || itMM->first == "PrimaryVertex fit-BeamFit" ||
                   itMM->first == "PrimaryVertex fit-Online" || itMM->first == "PrimaryVertex-DataBase" ||
                   itMM->first == "PrimaryVertex-BeamFit" || itMM->first == "PrimaryVertex-Online") {
          itMM->second = ibooker.book1D(name, title, 101, -5.05, 5.05);
        } else {
          //assert(0);
        }
      } else {
        //assert(0);
      }
      if (itMM->second != nullptr) {
        if (itMM->first == "Coordinate") {
          itMM->second->setAxisTitle(itM->first + "_{0} (cm)", 1);
        } else if (itMM->first == "PrimaryVertex fit-DataBase" || itMM->first == "PrimaryVertex fit-BeamFit" ||
                   itMM->first == "PrimaryVertex fit-Online" || itMM->first == "PrimaryVertex-DataBase" ||
                   itMM->first == "PrimaryVertex-BeamFit" || itMM->first == "PrimaryVertex-Online") {
          itMM->second->setAxisTitle(itMM->first + " " + itM->first + "_{0} (cm)", 1);
        }
        itMM->second->setAxisTitle("Entries", 2);
      }
    }

    //Making histos per Lumi
    // x,y,z,sigmaX,sigmaY,sigmaZ
    for (map<string, map<string, MonitorElement*> >::iterator itMM = itM->second.begin(); itMM != itM->second.end();
         itMM++) {
      if (itMM->first != "run") {
        for (map<string, MonitorElement*>::iterator itMMM = itMM->second.begin(); itMMM != itMM->second.end();
             itMMM++) {
          name = string("h") + itM->first + itMMM->first;
          title = itM->first + "_{0} " + itMMM->first;
          if (itMM->first == "lumi") {
            ibooker.setCurrentFolder(monitorName_ + "Debug");
            itMMM->second = ibooker.book1D(name, title, lastLumi - firstLumi + 1, firstLumi - 0.5, lastLumi + 0.5);
            itMMM->second->setEfficiencyFlag();
          } else if (itMM->first == "validation" && itMMM->first == "Lumibased Online-DataBase fit") {
            ibooker.setCurrentFolder(monitorName_ + "Validation");
            itMMM->second = ibooker.book1D(name, title, lastLumi - firstLumi + 1, firstLumi - 0.5, lastLumi + 0.5);
            itMMM->second->setEfficiencyFlag();
          } else if (itMM->first == "validation" && itMMM->first != "Lumibased Online-DataBase fit" &&
                     (itM->first == "x" || itM->first == "y" || itM->first == "z")) {
            ibooker.setCurrentFolder(monitorName_ + "Validation");
            itMMM->second = ibooker.book1D(name, title, lastLumi - firstLumi + 1, firstLumi - 0.5, lastLumi + 0.5);
            itMMM->second->setEfficiencyFlag();
          } else if (itMM->first == "validation" &&
                     (itM->first == "sigmaX" || itM->first == "sigmaY" || itM->first == "sigmaZ")) {
            ibooker.setCurrentFolder(monitorName_ + "Validation");
            itMMM->second = nullptr;
          } else {
            LogInfo("AlcaBeamMonitorClient") << "Unrecognized category " << itMM->first;
            // assert(0);
          }
          if (itMMM->second != nullptr) {
            if (itMMM->first.find('-') != string::npos) {
              itMMM->second->setAxisTitle(string("#Delta ") + itM->first + "_{0} (cm)", 2);
            } else {
              itMMM->second->setAxisTitle(itM->first + "_{0} (cm)", 2);
            }
            itMMM->second->setAxisTitle("Lumisection", 1);
          }
        }
      }
    }
  }

  // create and cd into new folder
  ibooker.setCurrentFolder(monitorName_ + "Validation");
  //Book histograms
  hD0Phi0_ = ibooker.bookProfile("hD0Phi0", "d_{0} vs. #phi_{0} (All Tracks)", 63, -3.15, 3.15, 100, -0.5, 0.5, "");
  hD0Phi0_->setAxisTitle("#phi_{0} (rad)", 1);
  hD0Phi0_->setAxisTitle("d_{0} (cm)", 2);

  ibooker.setCurrentFolder(monitorName_ + "Debug");
  hDxyBS_ = ibooker.book1D("hDxyBS", "dxy_{0} w.r.t. Beam spot (All Tracks)", 100, -0.1, 0.1);
  hDxyBS_->setAxisTitle("dxy_{0} w.r.t. Beam spot (cm)", 1);
}

//----------------------------------------------------------------------------------------------------------------------
std::shared_ptr<alcabeammonitor::BeamSpotInfo> AlcaBeamMonitor::globalBeginLuminosityBlock(
    const LuminosityBlock& iLumi, const EventSetup& iSetup) const {
  // Always create a beamspot group for each lumi weather we have results or not! Each Beamspot will be of unknown type!

  auto beamSpotInfo = std::make_shared<alcabeammonitor::BeamSpotInfo>();

  //Read BeamSpot from DB
  ESHandle<BeamSpotObjects> bsDBHandle;
  try {
    bsDBHandle = iSetup.getHandle(beamSpotToken_);
  } catch (cms::Exception& exception) {
    LogError("AlcaBeamMonitor") << exception.what();
    return nullptr;
  }
  if (bsDBHandle.isValid()) {  // check the product
    const BeamSpotObjects* spotDB = bsDBHandle.product();

    // translate from BeamSpotObjects to reco::BeamSpot
    BeamSpot::Point apoint(spotDB->x(), spotDB->y(), spotDB->z());

    BeamSpot::CovarianceMatrix matrix;
    for (int i = 0; i < reco::BeamSpot::dimension; ++i) {
      for (int j = 0; j < reco::BeamSpot::dimension; ++j) {
        matrix(i, j) = spotDB->covariance(i, j);
      }
    }

    beamSpotInfo->beamSpotMap_["DB"] =
        BeamSpot(apoint, spotDB->sigmaZ(), spotDB->dxdz(), spotDB->dydz(), spotDB->beamWidthX(), matrix);

    BeamSpot* aSpot = &(beamSpotInfo->beamSpotMap_["DB"]);

    aSpot->setBeamWidthY(spotDB->beamWidthY());
    aSpot->setEmittanceX(spotDB->emittanceX());
    aSpot->setEmittanceY(spotDB->emittanceY());
    aSpot->setbetaStar(spotDB->betaStar());

    if (spotDB->beamType() == 2) {
      aSpot->setType(reco::BeamSpot::Tracker);
    } else {
      aSpot->setType(reco::BeamSpot::Fake);
    }
    //LogInfo("AlcaBeamMonitor")
    //  << *aSpot << std::endl;
  } else {
    LogInfo("AlcaBeamMonitor") << "Database BeamSpot is not valid at lumi: " << iLumi.id().luminosityBlock();
  }
  return beamSpotInfo;
}

//----------------------------------------------------------------------------------------------------------------------
void AlcaBeamMonitor::analyze(const Event& iEvent, const EventSetup& iSetup) {
  //------ BeamFitter
  theBeamFitter_->readEvent(iEvent);
  //------ PVFitter
  thePVFitter_->readEvent(iEvent);

  auto beamSpotInfo = luminosityBlockCache(iEvent.getLuminosityBlock().index());
  if (beamSpotInfo->beamSpotMap_.find("DB") != beamSpotInfo->beamSpotMap_.end()) {
    //------ Tracks
    Handle<reco::TrackCollection> TrackCollection;
    iEvent.getByToken(trackLabel_, TrackCollection);
    const reco::TrackCollection* tracks = TrackCollection.product();
    for (reco::TrackCollection::const_iterator track = tracks->begin(); track != tracks->end(); ++track) {
      hD0Phi0_->Fill(track->phi(), -1 * track->dxy());
      hDxyBS_->Fill(-1 * track->dxy(beamSpotInfo->beamSpotMap_["DB"].position()));
    }
  }

  //------ Primary Vertices
  Handle<VertexCollection> PVCollection;
  if (iEvent.getByToken(primaryVertexLabel_, PVCollection)) {
    std::vector<alcabeammonitor::pvPosAndErr> vertices;
    vertices.reserve(PVCollection->size());
    for (const auto& pv : *PVCollection.product()) {
      if (pv.isFake() || pv.tracksSize() < 10)
        continue;
      vertices.emplace_back(pv);
    }
    vertices.shrink_to_fit();
    beamSpotInfo->vertices_.emplace_back(std::move(vertices));
  }

  if (beamSpotInfo->beamSpotMap_.find("SC") == beamSpotInfo->beamSpotMap_.end()) {
    //BeamSpot from file for this stream is = to the scalar BeamSpot
    if (Handle<BeamSpot> recoBeamSpotHandle = iEvent.getHandle(scalerLabel_)) {
      beamSpotInfo->beamSpotMap_["SC"] = *recoBeamSpotHandle;
      if (beamSpotInfo->beamSpotMap_["SC"].BeamWidthX() != 0) {
        beamSpotInfo->beamSpotMap_["SC"].setType(reco::BeamSpot::Tracker);
      } else {
        beamSpotInfo->beamSpotMap_["SC"].setType(reco::BeamSpot::Fake);
      }
    } else {
      LogWarning("AlcaBeamMonitor") << "No BeamSpot from scalers is available";
      return;
    }
  }
}

//----------------------------------------------------------------------------------------------------------------------
void AlcaBeamMonitor::globalEndLuminosityBlock(const LuminosityBlock& iLumi, const EventSetup& iSetup) {
  processedLumis_.push_back(iLumi.id().luminosityBlock());

  auto beamSpotInfo = luminosityBlockCache(iLumi.index());
  if (theBeamFitter_->runPVandTrkFitter()) {
    beamSpotInfo->beamSpotMap_["BF"] = theBeamFitter_->getBeamSpot();
  }
  theBeamFitter_->resetTrkVector();
  theBeamFitter_->resetLSRange();
  theBeamFitter_->resetRefTime();
  theBeamFitter_->resetPVFitter();

  if (thePVFitter_->runFitter()) {
    beamSpotInfo->beamSpotMap_["PV"] = thePVFitter_->getBeamSpot();
  }
  thePVFitter_->resetAll();

  //    "PV,BF..."      Value,Error
  map<std::string, pair<double, double> > resultsMap;
  vector<pair<double, double> > vertexResults;
  MonitorElement* histo = nullptr;
  for (vector<string>::iterator itV = varNamesV_.begin(); itV != varNamesV_.end(); itV++) {
    resultsMap.clear();
    for (BeamSpotContainer::iterator itBS = beamSpotInfo->beamSpotMap_.begin();
         itBS != beamSpotInfo->beamSpotMap_.end();
         itBS++) {
      if (itBS->second.type() == BeamSpot::Tracker) {
        if (*itV == "x") {
          resultsMap[itBS->first] = pair<double, double>(itBS->second.x0(), itBS->second.x0Error());
        } else if (*itV == "y") {
          resultsMap[itBS->first] = pair<double, double>(itBS->second.y0(), itBS->second.y0Error());
        } else if (*itV == "z") {
          resultsMap[itBS->first] = pair<double, double>(itBS->second.z0(), itBS->second.z0Error());
        } else if (*itV == "sigmaX") {
          resultsMap[itBS->first] = pair<double, double>(itBS->second.BeamWidthX(), itBS->second.BeamWidthXError());
        } else if (*itV == "sigmaY") {
          resultsMap[itBS->first] = pair<double, double>(itBS->second.BeamWidthY(), itBS->second.BeamWidthYError());
        } else if (*itV == "sigmaZ") {
          resultsMap[itBS->first] = pair<double, double>(itBS->second.sigmaZ(), itBS->second.sigmaZ0Error());
        } else {
          LogInfo("AlcaBeamMonitor") << "The histosMap_ has been built with the name " << *itV
                                     << " that I can't recognize!";
          //assert(0);
        }
      }
    }
    vertexResults.clear();

    for (const auto& itPV : beamSpotInfo->vertices_) {
      for (const auto& pv : itPV) {
        if (*itV == "x") {
          vertexResults.push_back(pv.xWithError());
        } else if (*itV == "y") {
          vertexResults.push_back(pv.yWithError());
        } else if (*itV == "z") {
          vertexResults.push_back(pv.zWithError());
        } else if (*itV != "sigmaX" && *itV != "sigmaY" && *itV != "sigmaZ") {
          LogInfo("AlcaBeamMonitor") << "The histosMap_ has been built with the name " << *itV
                                     << " that I can't recognize!";
          //assert(0);
        }
      }
    }

    for (multimap<string, string>::iterator itM = histoByCategoryNames_.begin(); itM != histoByCategoryNames_.end();
         itM++) {
      if ((histo = histosMap_[*itV][itM->first][itM->second]) == nullptr)
        continue;
      if (itM->second == "Coordinate") {
        if (beamSpotInfo->beamSpotMap_.find("DB") != beamSpotInfo->beamSpotMap_.end()) {
          histo->Fill(resultsMap["DB"].first);
        }
      } else if (itM->second == "PrimaryVertex fit-DataBase") {
        if (resultsMap.find("PV") != resultsMap.end() && resultsMap.find("DB") != resultsMap.end()) {
          histo->Fill(resultsMap["PV"].first - resultsMap["DB"].first);
        }
      } else if (itM->second == "PrimaryVertex fit-BeamFit") {
        if (resultsMap.find("PV") != resultsMap.end() && resultsMap.find("BF") != resultsMap.end()) {
          histo->Fill(resultsMap["PV"].first - resultsMap["BF"].first);
        }
      } else if (itM->second == "PrimaryVertex fit-Online") {
        if (resultsMap.find("PV") != resultsMap.end() && resultsMap.find("SC") != resultsMap.end()) {
          histo->Fill(resultsMap["PV"].first - resultsMap["SC"].first);
        }
      } else if (itM->second == "PrimaryVertex-DataBase") {
        if (resultsMap.find("PV") != resultsMap.end() && resultsMap.find("DB") != resultsMap.end()) {
          for (vector<pair<double, double> >::iterator itPV = vertexResults.begin(); itPV != vertexResults.end();
               itPV++) {
            histo->Fill(itPV->first - resultsMap["DB"].first);
          }
        }
      } else if (itM->second == "PrimaryVertex-BeamFit") {
        if (resultsMap.find("PV") != resultsMap.end() && resultsMap.find("BF") != resultsMap.end()) {
          for (vector<pair<double, double> >::iterator itPV = vertexResults.begin(); itPV != vertexResults.end();
               itPV++) {
            histo->Fill(itPV->first - resultsMap["BF"].first);
          }
        }
      } else if (itM->second == "PrimaryVertex-Online") {
        if (resultsMap.find("PV") != resultsMap.end() && resultsMap.find("SC") != resultsMap.end()) {
          for (vector<pair<double, double> >::iterator itPV = vertexResults.begin(); itPV != vertexResults.end();
               itPV++) {
            histo->Fill(itPV->first - resultsMap["SC"].first);
          }
        }
      } else if (itM->second == "Lumibased BeamSpotFit") {
        if (resultsMap.find("BF") != resultsMap.end()) {
          histo->setBinContent(iLumi.id().luminosityBlock(), resultsMap["BF"].first);
          histo->setBinError(iLumi.id().luminosityBlock(), resultsMap["BF"].second);
        }
      } else if (itM->second == "Lumibased PrimaryVertex") {
        if (resultsMap.find("PV") != resultsMap.end()) {
          histo->setBinContent(iLumi.id().luminosityBlock(), resultsMap["PV"].first);
          histo->setBinError(iLumi.id().luminosityBlock(), resultsMap["PV"].second);
        }
      } else if (itM->second == "Lumibased DataBase") {
        if (resultsMap.find("DB") != resultsMap.end()) {
          histo->setBinContent(iLumi.id().luminosityBlock(), resultsMap["DB"].first);
          histo->setBinError(iLumi.id().luminosityBlock(), resultsMap["DB"].second);
        }
      } else if (itM->second == "Lumibased Online") {
        if (resultsMap.find("SC") != resultsMap.end()) {
          histo->setBinContent(iLumi.id().luminosityBlock(), resultsMap["SC"].first);
          histo->setBinError(iLumi.id().luminosityBlock(), resultsMap["SC"].second);
        }
      } else if (itM->second == "Lumibased PrimaryVertex-DataBase fit") {
        if (resultsMap.find("PV") != resultsMap.end() && resultsMap.find("DB") != resultsMap.end()) {
          histo->setBinContent(iLumi.id().luminosityBlock(), resultsMap["PV"].first - resultsMap["DB"].first);
          histo->setBinError(iLumi.id().luminosityBlock(),
                             std::sqrt(std::pow(resultsMap["PV"].second, 2) + std::pow(resultsMap["DB"].second, 2)));
        }
      } else if (itM->second == "Lumibased PrimaryVertex-Online fit") {
        if (resultsMap.find("PV") != resultsMap.end() && resultsMap.find("SC") != resultsMap.end()) {
          histo->setBinContent(iLumi.id().luminosityBlock(), resultsMap["PV"].first - resultsMap["SC"].first);
          histo->setBinError(iLumi.id().luminosityBlock(),
                             std::sqrt(std::pow(resultsMap["PV"].second, 2) + std::pow(resultsMap["SC"].second, 2)));
        }
      } else if (itM->second == "Lumibased Online-DataBase fit") {
        if (resultsMap.find("SC") != resultsMap.end() && resultsMap.find("DB") != resultsMap.end()) {
          histo->setBinContent(iLumi.id().luminosityBlock(), resultsMap["SC"].first - resultsMap["DB"].first);
          histo->setBinError(iLumi.id().luminosityBlock(),
                             std::sqrt(std::pow(resultsMap["SC"].second, 2) + std::pow(resultsMap["DB"].second, 2)));
        }
      } else if (itM->second == "Lumibased PrimaryVertex-DataBase") {
        if (resultsMap.find("DB") != resultsMap.end() && !vertexResults.empty()) {
          for (vector<pair<double, double> >::iterator itPV = vertexResults.begin(); itPV != vertexResults.end();
               itPV++) {
            histo->setBinContent(iLumi.id().luminosityBlock(), (*itPV).first - resultsMap["DB"].first);
            histo->setBinError(iLumi.id().luminosityBlock(),
                               std::sqrt(std::pow((*itPV).second, 2) + std::pow(resultsMap["DB"].second, 2)));
          }
        }
      } else if (itM->second == "Lumibased PrimaryVertex-Online") {
        if (resultsMap.find("SC") != resultsMap.end() && !vertexResults.empty()) {
          for (vector<pair<double, double> >::iterator itPV = vertexResults.begin(); itPV != vertexResults.end();
               itPV++) {
            histo->setBinContent(iLumi.id().luminosityBlock(), (*itPV).first - resultsMap["SC"].first);
            histo->setBinError(iLumi.id().luminosityBlock(),
                               std::sqrt(std::pow((*itPV).second, 2) + std::pow(resultsMap["SC"].second, 2)));
          }
        }
      }
      //      else if(itM->second == "Lumibased Online-DataBase"){
      //      if(resultsMap.find("SC") != resultsMap.end() && resultsMap.find("DB") != resultsMap.end()){
      //        itHHH->second->Fill(bin,resultsMap["SC"].first-resultsMap["DB"].first);
      //      }
      //    }
      else {
        LogInfo("AlcaBeamMonitor") << "The histosMap_ have a histogram named " << itM->second
                                   << " that I can't recognize in this loop!";
        //assert(0);
      }
    }
  }
}

void AlcaBeamMonitor::dqmEndRun(edm::Run const&, edm::EventSetup const&) {
  if (processedLumis_.empty()) {
    return;
  }

  const double bigNumber = 1000000.;
  std::sort(processedLumis_.begin(), processedLumis_.end());
  int firstLumi = *processedLumis_.begin();
  int lastLumi = *(--processedLumis_.end());

  for (HistosContainer::iterator itH = histosMap_.begin(); itH != histosMap_.end(); itH++) {
    for (map<string, map<string, MonitorElement*> >::iterator itHH = itH->second.begin(); itHH != itH->second.end();
         itHH++) {
      double min = bigNumber;
      double max = -bigNumber;
      double minDelta = bigNumber;
      double maxDelta = -bigNumber;
      //      double minDeltaProf = bigNumber;
      //      double maxDeltaProf = -bigNumber;
      if (itHH->first != "run") {
        for (map<string, MonitorElement*>::iterator itHHH = itHH->second.begin(); itHHH != itHH->second.end();
             itHHH++) {
          if (itHHH->second != nullptr) {
            for (int bin = 1; bin <= itHHH->second->getTH1()->GetNbinsX(); bin++) {
              if (itHHH->second->getTH1()->GetBinError(bin) != 0 || itHHH->second->getTH1()->GetBinContent(bin) != 0) {
                if (itHHH->first == "Lumibased BeamSpotFit" || itHHH->first == "Lumibased PrimaryVertex" ||
                    itHHH->first == "Lumibased DataBase" || itHHH->first == "Lumibased Online") {
                  if (min > itHHH->second->getTH1()->GetBinContent(bin)) {
                    min = itHHH->second->getTH1()->GetBinContent(bin);
                  }
                  if (max < itHHH->second->getTH1()->GetBinContent(bin)) {
                    max = itHHH->second->getTH1()->GetBinContent(bin);
                  }
                } else if (itHHH->first == "Lumibased PrimaryVertex-DataBase fit" ||
                           itHHH->first == "Lumibased PrimaryVertex-Online fit" ||
                           itHHH->first == "Lumibased Online-DataBase fit" ||
                           itHHH->first == "Lumibased PrimaryVertex-DataBase" ||
                           itHHH->first == "Lumibased PrimaryVertex-Online") {
                  if (minDelta > itHHH->second->getTH1()->GetBinContent(bin)) {
                    minDelta = itHHH->second->getTH1()->GetBinContent(bin);
                  }
                  if (maxDelta < itHHH->second->getTH1()->GetBinContent(bin)) {
                    maxDelta = itHHH->second->getTH1()->GetBinContent(bin);
                  }
                } else {
                  LogInfo("AlcaBeamMonitorClient") << "The histosMap_ have a histogram named " << itHHH->first
                                                   << " that I can't recognize in this loop!";
                  // assert(0);
                }
              }
            }
          }
        }
        for (map<string, MonitorElement*>::iterator itHHH = itHH->second.begin(); itHHH != itHH->second.end();
             itHHH++) {
          if (itHHH->second != nullptr) {
            if (itHHH->first == "Lumibased BeamSpotFit" || itHHH->first == "Lumibased PrimaryVertex" ||
                itHHH->first == "Lumibased DataBase" || itHHH->first == "Lumibased Online") {
              if ((max == -bigNumber && min == bigNumber) || max - min == 0) {
                itHHH->second->getTH1()->SetMinimum(itHHH->second->getTH1()->GetMinimum() - 0.01);
                itHHH->second->getTH1()->SetMaximum(itHHH->second->getTH1()->GetMaximum() + 0.01);
              } else {
                itHHH->second->getTH1()->SetMinimum(min - 0.1 * (max - min));
                itHHH->second->getTH1()->SetMaximum(max + 0.1 * (max - min));
              }
            } else if (itHHH->first == "Lumibased PrimaryVertex-DataBase fit" ||
                       itHHH->first == "Lumibased PrimaryVertex-Online fit" ||
                       itHHH->first == "Lumibased Online-DataBase fit" ||
                       itHHH->first == "Lumibased PrimaryVertex-DataBase" ||
                       itHHH->first == "Lumibased PrimaryVertex-Online") {
              if ((maxDelta == -bigNumber && minDelta == bigNumber) || maxDelta - minDelta == 0) {
                itHHH->second->getTH1()->SetMinimum(itHHH->second->getTH1()->GetMinimum() - 0.01);
                itHHH->second->getTH1()->SetMaximum(itHHH->second->getTH1()->GetMaximum() + 0.01);
              } else {
                itHHH->second->getTH1()->SetMinimum(minDelta - 2 * (maxDelta - minDelta));
                itHHH->second->getTH1()->SetMaximum(maxDelta + 2 * (maxDelta - minDelta));
              }
            } else {
              LogInfo("AlcaBeamMonitorClient") << "The histosMap_ have a histogram named " << itHHH->first
                                               << " that I can't recognize in this loop!";
            }
            itHHH->second->getTH1()->GetXaxis()->SetRangeUser(firstLumi - 0.5, lastLumi + 0.5);
          }
        }
      }
    }
  }
}
DEFINE_FWK_MODULE(AlcaBeamMonitor);
