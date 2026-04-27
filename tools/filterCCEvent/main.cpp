#include <iomanip>
#include <unordered_map>
#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>

#include <TFile.h>
#include <TTree.h>
#include "ANNIEEventReader.h"
#include "LAPPDMetaReader.h"
#include "TankClusterReader.h"
#include "MRDClusterReader.h"
#include "TPaveText.h"
#include "RootTreeReader.h"

#include "INIReader.h"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void ReadChannels(const std::string& filename,
                  std::vector<int>& verticalChannels,
                  std::vector<int>& horizontalChannels);
 


//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void FilterCCEvent(const INIReader& iniReader);

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

int main(int argc, char* argv[]) 
{
   if (argc < 2) {
      std::cerr << "Usage: ./filterCCEvent <config.ini>" << std::endl;
      return 1;
   }

   INIReader iniReader(argv[1]);

   if (iniReader.ParseError() != 0) {
      std::cerr << "Could not load config file: " << argv[1] << std::endl;
      return 1;
   }

   FilterCCEvent(iniReader);

   return 0;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
void FilterCCEvent(const INIReader& iniReader)
{
   std::string inputFileName  = iniReader.Get("input",  "rootFile", "");
   std::string mrdInfoFile    = iniReader.Get("input",  "mrdFile", "MRDInfo.txt");
   std::string outputFileName = iniReader.Get("output", "rootFile", "");
   const bool useMC           = iniReader.GetBoolean("input", "useMC", false);
   
   const bool cutRequireLAPPDMeta      = iniReader.GetBoolean("cuts", "requireLAPPDMeta", true);
   const bool cutUseSelectedLAPPD      = iniReader.GetBoolean("cuts", "useSelectedLAPPD", false);
   const int  selectedLAPPDID          = iniReader.GetInteger("cuts", "selectedLAPPDID", -1);

   const bool cutBeamOK                = (!useMC) && iniReader.GetBoolean("cuts", "beamOK", true);
   const bool cutBRFWindow             = (!useMC) && iniReader.GetBoolean("cuts", "brfWindow", false);
   const bool cutPPSMissing            = (!useMC) && iniReader.GetBoolean("cuts", "ppsMissing", false);

   const bool cutPairedEvent           = iniReader.GetBoolean("cuts", "pairedEvent", true);
   const bool cutPromptPMTCluster      = iniReader.GetBoolean("cuts", "promptPMTCluster", true);
   const bool cutHighQualityPMTCluster = iniReader.GetBoolean("cuts", "highQualityPMTCluster", true);
   const bool cutMRDTrackReconstructed = iniReader.GetBoolean("cuts", "mrdTrackReconstructed", true);
   const bool cutTankMRDCoinc          = iniReader.GetBoolean("cuts", "tankMRDCoinc", true);
   const bool cutMuonTopology          = iniReader.GetBoolean("cuts", "muonTopology", false);
   const bool cutNoVeto                = iniReader.GetBoolean("cuts", "noVeto", false);

   const double promptWindowMax        = iniReader.GetReal("cutValues", "promptWindowMax", 2000.0);
   const double chargeBalanceMax       = iniReader.GetReal("cutValues", "chargeBalanceMax", 0.2);
   const double mrdTimeWindow          = iniReader.GetReal("cutValues", "mrdTimeWindow", 30.0);
   const int    mrdMinSideHits         = iniReader.GetInteger("cutValues", "mrdMinSideHits", 2);
   const int    mrdMinTopHits          = iniReader.GetInteger("cutValues", "mrdMinTopHits", 2);
   const double pmtPEThreshold         = iniReader.GetReal("cutValues", "pmtPEThreshold", 5.0);

   const double brfMin                 = iniReader.GetReal("cutValues", "brfMin", 5.0);
   const double brfMax                 = iniReader.GetReal("cutValues", "brfMax", 30.0);

   if (inputFileName.empty()) {
      std::cerr << "Error: input.rootFile is empty in config." << std::endl;
      return;
   }

   if (outputFileName.empty()) {
      std::cerr << "Error: output.rootFile is empty in config." << std::endl;
      return;
   }

   const std::string& treeName = "Event";
   RootTreeReader treeReader(inputFileName.c_str(), treeName.c_str());
   
   if(!treeReader.IsValid())
      return;
   
   TTree* tree = treeReader.GetTree(); 
   
   // Output file 
   TFile* outputFile = new TFile(outputFileName.c_str(), "RECREATE");

   if (!outputFile || outputFile->IsZombie()) {
      std::cerr << "Could not create output file: " << outputFileName << std::endl;
      return;
   }
   

   // Clone input tree structure, but with 0 entries
   TTree* outputTree = tree->CloneTree(0);
   outputTree->SetName("Event");
   outputTree->SetTitle("Filtered Event Tree");
   
   Long64_t nEntries = tree->GetEntries();
   std::cout << "Number of events in this tree: " << nEntries << std::endl;

   Long64_t nPassedEvents = 0;
   
   
   int beam_ok = 1;
   double BRFFirstPeakFit = 0.0;

   if (!useMC) {
      tree->SetBranchAddress("beam_ok", &beam_ok);
      tree->SetBranchAddress("BRFFirstPeakFit", &BRFFirstPeakFit);
   }

   // Initialize readers
   LAPPDMetaReader metaReader;

   if (!useMC) {
      metaReader.Init(tree);
   }

   ANNIEEventReader eventReader; 
   eventReader.Init(tree);

   TankClusterReader tankClusterReader;
   tankClusterReader.Init(tree);
   
   MRDClusterReader mrdClusterReader;
   mrdClusterReader.Init(tree);


   //For MRDtrackReconstructed cut
   std::vector<int> sideMRDPMTs;
   std::vector<int> topMRDPMTs;
   ReadChannels(mrdInfoFile, sideMRDPMTs, topMRDPMTs);
   //std::cout << "Vertical size: " << sideMRDPMTs.size() << std::endl;
   //std::cout << "Horizontal size: " << topMRDPMTs.size() << std::endl;

   
   for (Long64_t i = 0; i < nEntries; ++i) 
   {
      //std::cout<<"EVENT "<<i<<std::endl;
      
      tree->GetEntry(i);
      
      //Skipped events 1 
      //No need to look at this event if no lappd data
      if (cutRequireLAPPDMeta && metaReader.mLAPPD_ID->size() == 0) {
         continue;
      }
      
      //Skipped events 2
      //No need to look at this event if beam is not ok
      if (cutBeamOK && !beam_ok) {
          continue;
      }
      
      //Skipped events 3
      //No need to look at this event if brf is not ok
      if (cutBRFWindow) {
         double brf_us = BRFFirstPeakFit / 1000.0;

         if (brf_us < brfMin || brf_us > brfMax) {
            continue;
         }
      } 
      
      //Skipped events 4
      // No need to look at this event if selected LAPPD has no data
      if (cutUseSelectedLAPPD) {
         auto ite = std::find(metaReader.mLAPPD_ID->begin(),
                              metaReader.mLAPPD_ID->end(),
                              selectedLAPPDID);

         if (ite == metaReader.mLAPPD_ID->end()) {
            continue;
         }
      }
      
         
      //Skipped events 5
      // 3. PPS missing check. if there is a missing PPS, it will be 1 in the vector. 
      // All should be zero!
      // No need to look at this event if PPS missing
      if (!useMC && cutPPSMissing) {

         bool hasPPSMissing = false;

         for (size_t j = 0; j < metaReader.mLAPPD_TSPPSMissing->size(); ++j) {
            if (metaReader.mLAPPD_TSPPSMissing->at(j) != 0) {
               hasPPSMissing = true;
               break;
            }
         }

         if (hasPPSMissing) {
            continue;
         }
      }
 
  
      //------------------------------------------------------------------------
      // The belows are Event-level cuts
      ETankClusters eTankClusters;
      tankClusterReader.ReadEntry(i, eTankClusters);

      EMRDClusters eMRDClusters;
      mrdClusterReader.ReadEntry(i, eMRDClusters);
            
      // Cut 0: Paired
      bool isPairedEvent = false;
      if (eventReader.HasLAPPD() == 1 && 
          eventReader.HasMRD()   == 1   && 
          eventReader.HasTank()  == 1 )
      {
         
         isPairedEvent = true;
      }
         
      
      // Cut 1: Any PMT cluster 
      // Find the cluster with the maximum charge (PE) in the initial time window
      bool hasClusterInPromptWindow = false;
      double maxPENumber = -1.0;

      const TankCluster* clusterPtrWithMaxPE = nullptr;
      for (const TankCluster& cl : eTankClusters.Get() )
      {
         
         if (cl.GetClusterTime() < promptWindowMax) //initial window
         {
          
            hasClusterInPromptWindow = true;

            if (cl.GetMaxPE() > maxPENumber) 
            {
                maxPENumber  = cl.GetMaxPE();
                clusterPtrWithMaxPE = &cl;
            }
            
            //std::cout<<"Max PE in this cluster : "<<cl.GetMaxPE()<<std::endl;
            //for(const PMTHit& hit: cl.GetHits())
            //std::cout<<hit.GetPE()<<std::endl;
                  
         }
           
      }
     
      //std::cout<<"The higest PE number in all clusters: "<<maxPENumber<<std::endl;
       
     
      // Cut 2: High-quality PMT cluster
      // Check if the previously selected cluster is a high-quality PMT cluster
      bool highQualityCluster = false;   
      if (hasClusterInPromptWindow && clusterPtrWithMaxPE )
      {
         
         if (clusterPtrWithMaxPE->GetChargeBalance() < chargeBalanceMax)
            highQualityCluster = true;
      }
         

      // Cut 3: MRD track reconstructed
      bool mrdTrackReconstructed = false;

      for (const MRDCluster& cluster : eMRDClusters.Get()) {

         // Her hit için: (time, type)
         // type: 0 -> side, 1 -> top, -1 -> ilgisiz
         std::vector<std::pair<double, int>> taggedHits;

         for (const MRDHit& hit : cluster.GetHits()) {

            int chankey = hit.GetChankey();
            double time = hit.GetTime();

            if (std::find(sideMRDPMTs.begin(), sideMRDPMTs.end(), chankey) != sideMRDPMTs.end()) {
               taggedHits.emplace_back(time, 0);
            }
            else if (std::find(topMRDPMTs.begin(), topMRDPMTs.end(), chankey) != topMRDPMTs.end()) {
               taggedHits.emplace_back(time, 1);
            }
         }

         if (taggedHits.size() < 4) continue;

         // Zamana göre sırala
         std::sort(taggedHits.begin(), taggedHits.end(),
                   [](const auto& a, const auto& b) {
                      return a.first < b.first;
                   });

         // Sliding window
         for (size_t i = 0; i < taggedHits.size(); ++i) {

            int nSideHits = 0;
            int nTopHits  = 0;

            for (size_t j = i; j < taggedHits.size(); ++j) {

               double dt = taggedHits[j].first - taggedHits[i].first;

               if (dt > mrdTimeWindow) break;

               if (taggedHits[j].second == 0) ++nSideHits;
               else if (taggedHits[j].second == 1) ++nTopHits;

               if (nSideHits >= mrdMinSideHits && nTopHits >= mrdMinTopHits) {
                  mrdTrackReconstructed = true;
                  break;
               }
            }

            if (mrdTrackReconstructed) break;
         }

         if (mrdTrackReconstructed) break;  // bir cluster yeterli
      }
      
      
      
      
      
      // Cut 4: In-time MRD coincidence
      bool tankMRDCoinc = false;
      if (eventReader.GetTankMRDCoinc() == 1)
         tankMRDCoinc = true;
      
      
      
      //Cut 5: Muon topology cut
      bool muonTopology = false;
      
      //neigbors of lappd 39
      std::vector<int> pmtIDs {462, 428, 406, 412};
      //std::vector<int> pmtIDs {383, 389, 440, 441};

      std::unordered_map<int, bool> passed;
      for (int id : pmtIDs) 
         passed[id] = false;

      for (const TankCluster& cluster : eTankClusters.Get())
      {
         if (cluster.GetClusterTime() > 2000.0) //initial window
            continue;
            
         for (const PMTHit& hit : cluster.GetHits())
         {
             double q = hit.GetPE();
             int id   = hit.GetDetID();

             auto it = passed.find(id);
             if (it != passed.end() && q > pmtPEThreshold)
             {
               it->second = true;
             }
         }
      }

      bool missing = false;
      for (auto& kv : passed)
      {
        if (!kv.second)
        {
          missing = true;
          break;
        }
      }

      if (!missing) 
         muonTopology = true;



      //Cut 6: No hit in FMV
      bool noVeto = false;
      if (eventReader.NoVeto() == 1)
         noVeto = true;
 
      //------------------------------------------------------------------------
 
 
      bool passedAllCuts = true;

      if (cutPairedEvent)
         passedAllCuts = passedAllCuts && isPairedEvent;

      if (cutPromptPMTCluster)
         passedAllCuts = passedAllCuts && hasClusterInPromptWindow;

      if (cutHighQualityPMTCluster)
         passedAllCuts = passedAllCuts && highQualityCluster;

      if (cutMRDTrackReconstructed)
         passedAllCuts = passedAllCuts && mrdTrackReconstructed;

      if (cutTankMRDCoinc)
         passedAllCuts = passedAllCuts && tankMRDCoinc;

      if (cutMuonTopology)
         passedAllCuts = passedAllCuts && muonTopology;

      if (cutNoVeto)
         passedAllCuts = passedAllCuts && noVeto;

      if (passedAllCuts) {
         outputTree->Fill();
         ++nPassedEvents;
      }
    
   
   
 
   }//event loop end
   
   outputFile->cd();
   outputTree->Write();
   outputFile->Close();

   std::cout << "Number of events passing all cuts: " << nPassedEvents << std::endl;
   std::cout << "Filtered tree written to: " << outputFileName << std::endl;


}


//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void ReadChannels(const std::string& filename,
                  std::vector<int>& verticalChannels,
                  std::vector<int>& horizontalChannels)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "MRD File cannot be opened!" << std::endl;
        return;
    }

    std::string line;

    // header'ı atla
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::stringstream ss(line);

        int channel_num;
        int detector_system;
        int orientation;

        // sadece ilk 3 column'u oku
        ss >> channel_num >> detector_system >> orientation;

        if (orientation == 0) {
            verticalChannels.push_back(channel_num);
        }
        else if (orientation == 1) {
            horizontalChannels.push_back(channel_num);
        }
    }

    file.close();
}

