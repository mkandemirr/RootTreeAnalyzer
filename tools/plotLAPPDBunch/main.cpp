#include <TFile.h>
#include <TTree.h>
#include <TH1D.h>
#include <THStack.h>
#include <TCanvas.h>
#include <TLegend.h>
#include <algorithm>
#include <iostream>
#include "ANNIEEventReader.h"
#include "LAPPDMetaReader.h"
#include "TankClusterReader.h"
#include "MRDClusterReader.h"
#include <iomanip>
#include "TPaveText.h"
#include "RootTreeReader.h"
#include <TRootCanvas.h>
#include <TApplication.h>

#include "LAPPDPulse.h"
#include "LAPPDPulseReader.h"

#include "LAPPDHit.h"
#include "LAPPDHitReader.h"

#include <unordered_map>
#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void PlotTimestampMinusBeamgate(const char* fileName);

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......



//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
                        
std::string int128_to_string(__int128 value);


//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

static inline __int128 to_i128(uint64_t x) { return static_cast<__int128>(x); }

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

static inline __int128 to_i128(int64_t  x) { return static_cast<__int128>(x); }

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

static inline __int128 to_i128(int  x) { return static_cast<__int128>(x); }

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

// 1 tick = 3.125 ns = 3125 ps (EXACT)
static inline __int128 ticks_to_ps_i128(__int128 ticks) {
  return ticks * 3125;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

// BG_unix(ps) = (BG_raw + BG_corr)*3125ps + Offset_ns*1000 - OSInMinusPS_ps
static __int128 compute_BG_unix_ps(
    uint64_t BG_raw,
    int  BG_corr,
    uint64_t Offset_ns,
    int OSInMinusPS_ps
) {
  __int128 ticks = to_i128(BG_raw) + to_i128(BG_corr) ;
  __int128 ps_from_ticks = ticks_to_ps_i128(ticks);
  __int128 ps_offset = to_i128(Offset_ns) * 1000; // ns -> ps
  __int128 ps_os = to_i128(OSInMinusPS_ps);
  return ps_from_ticks + ps_offset - ps_os;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

static __int128 compute_TS_unix_ps(
    uint64_t TS_raw,
    int  TS_corr,
    uint64_t Offset_ns,
    int OSInMinusPS_ps
) {
    __int128 ticks = to_i128(TS_raw) + to_i128(TS_corr);
    __int128 ps_from_ticks = ticks_to_ps_i128(ticks);
    __int128 ps_offset = to_i128(Offset_ns) * 1000; // ns → ps
    __int128 ps_os = to_i128(OSInMinusPS_ps);

    return ps_from_ticks + ps_offset - ps_os;
}



//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

int main(int argc, char* argv[]) 
{
    const char* fileName = "ANNIETree.root"; //default
    if (argc > 1) {
      fileName = argv[1];
    }

    PlotTimestampMinusBeamgate(fileName);
    return 0;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void PlotTimestampMinusBeamgate(const char* fileName) 
{

   TApplication *app = new TApplication("Root Application", nullptr, nullptr);
    
   RootTreeReader treeReader(fileName,"Event");
   
   if(!treeReader.IsValid())
      return;
   
   TTree* tree = treeReader.GetTree(); 
   
   
   // Initialize readers
   ANNIEEventReader eventReader;
   eventReader.Init(tree);
   
   // Initialize readers
   LAPPDMetaReader metaReader; 
   metaReader.Init(tree);
   
   LAPPDPulseReader pulseReader;
   pulseReader.Init(tree);
   
   LAPPDHitReader hitReader;
   hitReader.Init(tree);

   double BRFFirstPeakFit;
   tree->SetBranchAddress("BRFFirstPeakFit", &BRFFirstPeakFit);
   
   // Get entry count
   Long64_t nEntries = tree->GetEntries();
   std::cout << "Number of events in this tree: " << nEntries << std::endl;

   // Create histograms
   
   TH1D* hist   = new TH1D("High-quality PMT cluster", "",   700, 7500, 9600);
   
   TH1D* hist2 = new TH1D(
    "hEarliestLAPPDPulseBeamgate",
    "Earliest LAPPD pulse time relative to beamgate;Time from beamgate [ns];Events",
    1000, 7500, 9500);
    
   TH1D* hist3 = new TH1D(
   "hEarliestLAPPDHitBeamgate",
   "Earliest LAPPD hit time relative to beamgate;Time from beamgate [ns];Events",
   700, 7500, 9600);
 
   
   std::map<int, long long> lappdCounts;   // ID -> kaç kez görüldü

   int selectedLAPPDID = -1;  // -1 means all active
   
   
   //nEntries = 100;
   // Loop over entries and fill
   for (Long64_t i = 0; i < nEntries; ++i) 
   {
      //std::cout<<"EVENT "<<i<<std::endl;
      
      tree->GetEntry(i);
      
      /*
      // ----------------------------------------------------
      // Find primary trigger time = UBT time
      // ----------------------------------------------------
      int primaryTriggerWord = eventReader.GetPrimaryTriggerWord();

      const std::vector<uint64_t>& groupedTriggerTime =
          *(eventReader.GetGroupedTriggerTime());

      const std::vector<uint32_t>& groupedTriggerWord =
          *(eventReader.GetGroupedTriggerWord());

      bool foundPrimary = false;
      uint64_t primaryTriggerTime_ns = 0;

      for (size_t k = 0; k < groupedTriggerWord.size(); ++k) {
          if ((int)groupedTriggerWord[k] == primaryTriggerWord) {
              primaryTriggerTime_ns = groupedTriggerTime[k];
              foundPrimary = true;
              break;
          }
      }

      if (!foundPrimary)
          continue;
     
      __int128 primaryTriggerTime_ps = to_i128(primaryTriggerTime_ns) * 1000;      
     */ 
      
      // count the IDs before skipping any events.
      for (int id : *metaReader.mLAPPD_ID)
         lappdCounts[id]++;
      
      
      //Skipped events 2
      //it works only if one LAPPD is active
      if (selectedLAPPDID != -1) //-1 means all active
      {   
         //Check the selected LAPPD exist in this event, if not skip
         auto ite = std::find(metaReader.mLAPPD_ID->begin(), 
                              metaReader.mLAPPD_ID->end(), 
                              selectedLAPPDID);
                             
         bool notFound = ( ite == metaReader.mLAPPD_ID->end() );

         
         if (notFound) continue;
         //throw std::runtime_error("selected id not foundQ");
      }
      
                  
      //------------------------------------------------------------------------
 
 
      //LAPPD meta reader 
      const std::vector<int> lappd_ids      = *(metaReader.mLAPPD_ID); 
      const std::vector<uint64_t> offset    = *(metaReader.mLAPPD_Offset);
      const std::vector<int> os_InMinus_ps  = *(metaReader.mLAPPD_OSInMinusPS);
       
      //Timestamp   
      const std::vector<uint64_t> ts_raw    = *(metaReader.mLAPPD_Timestamp_Raw);
      const std::vector<int> ts_corr        = *(metaReader.mLAPPD_TSCorrection); 
      //Beamgate  
      const std::vector<uint64_t> bg_raw    = *(metaReader.mLAPPD_Beamgate_Raw);
      const std::vector<int>  bg_corr       = *(metaReader.mLAPPD_BGCorrection);


      { //check vector size mismatch
         const size_t n_offset   = offset.size();
         const size_t n_os       = os_InMinus_ps.size();
         const size_t n_ids      = lappd_ids.size();
         const size_t n_ts_raw   = ts_raw.size();
         const size_t n_ts_corr  = ts_corr.size();
         const size_t n_bg_raw   = bg_raw.size();
         const size_t n_bg_corr  = bg_corr.size();

         // Hepsi eşit mi?
         if (!(n_offset == n_os &&
               n_offset == n_ids &&
               n_offset == n_ts_raw &&
               n_offset == n_ts_corr &&
               n_offset == n_bg_raw &&
               n_offset == n_bg_corr))
         {
           std::ostringstream oss;
           oss << "LAPPD metadata vector sizes are NOT equal!\n"
               << "offset.size()      = " << n_offset  << "\n"
               << "os_InMinus_ps.size() = " << n_os      << "\n"
               << "lappd_ids.size()    = " << n_ids     << "\n"
               << "ts_raw.size()       = " << n_ts_raw  << "\n"
               << "ts_corr.size()      = " << n_ts_corr << "\n"
               << "bg_raw.size()       = " << n_bg_raw  << "\n"
               << "bg_corr.size()      = " << n_bg_corr << "\n";

           throw std::runtime_error(oss.str());
         }

      }
 
      int numberOfActiveLAPPPD = metaReader.mLAPPD_ID->size();  
              
      double t0 = BRFFirstPeakFit * 0.001; // ps -> ns varsayımı
      
      ELAPPDHits eLAPPDHits;
      hitReader.ReadEntry(i, eLAPPDHits);
      
      //earliest pulse time
      ELAPPDPulses eLAPPDPulses;
      pulseReader.ReadEntry(i, eLAPPDPulses);
      
      
      
   
      std::unordered_map<int, double> lappd_dt_map;

      for (size_t j = 0; j < numberOfActiveLAPPPD; ++j)
      {
          int id = lappd_ids.at(j);

          __int128 ts_unix_ps = compute_TS_unix_ps(
              ts_raw.at(j),
              ts_corr.at(j),
              offset.at(j),
              os_InMinus_ps.at(j)
          );

          __int128 bg_unix_ps = compute_BG_unix_ps(
              bg_raw.at(j),
              bg_corr.at(j),
              offset.at(j),
              os_InMinus_ps.at(j)
          );

          __int128 dt_ps = ts_unix_ps - bg_unix_ps;
          double dt_ns = static_cast<double>(dt_ps) / 1000.0;

          lappd_dt_map[id] = dt_ns;
      }
      
      
      
   // Earliest LAPPD hit'i beamgate'e göre bul.
   // Her hit için:
   //   absolute hit time wrt beamgate = (timestamp - beamgate) + peakTime
   double earliestHitTimeBeamgate = 1e9;
   double earliestHitDtNs = 1e9;
   int earliestHitID = -1;

   for (const LAPPDHit& hit : eLAPPDHits.GetHits())
   {
       int id = hit.GetLAPPDID();

       auto it = lappd_dt_map.find(id);
       if (it == lappd_dt_map.end())
           continue;

       double dt_ns = it->second;

       double peakTime = std::min(
           hit.GetP1PeakTime(),
           hit.GetP2PeakTime()
       );

       double hitTimeBeamgate = dt_ns + peakTime - t0;

       if (hitTimeBeamgate < earliestHitTimeBeamgate)
       {
           earliestHitTimeBeamgate = hitTimeBeamgate;
           earliestHitDtNs = dt_ns;
           earliestHitID = id;
       }
   }

   if (earliestHitID != -1)
   {
       hist3->Fill(earliestHitTimeBeamgate);
       hist->Fill(earliestHitDtNs-t0);
   }
      
      
   lappd_dt_map.clear();

      
     // if (earliest_dt_ns < 1e9) {
     //    hist->Fill(earliest_dt_ns-t0);
     // }
      
      
      
       
   
   
      //std::cout<<"End of event "<<i<<std::endl;
   } //end of event loop
   
   
  
   
   hist->Draw();
   //hist2->Draw();
   //hist3->Draw();

   std::cout << "==== LAPPD ID counters ====\n";
   for (const auto& [id, cnt] : lappdCounts) {
       double ratio = (nEntries > 0) ? (static_cast<double>(cnt) / nEntries) * 100.0 : 0.0;
       std::cout << "ID " << id << " : " << cnt << " , ratio = " << ratio << "%\n";
   }

   
   std::cout << "Selected LAPPD ID: " << selectedLAPPDID <<std::endl;
   
  

   
   // -----------------------------------------------------------------------------------
    // Pencereyi kapatınca uygulamayı sonlandır
    // -----------------------------------------------------------------------------------
    TRootCanvas *rc = (TRootCanvas *)gPad->GetCanvas()->GetCanvasImp();
    rc->Connect("CloseWindow()", "TApplication", gApplication, "Terminate()");
    app->Run();
 
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......






// __int128 tipi C++'ta cout ile dogrudan yazdirilamaz.
// Bu fonksiyon, __int128 sayiyi string'e cevirerek
// guvenli ve kayipsiz sekilde yazdirmayi saglar.
std::string int128_to_string(__int128 value) {

    // Eger sayi 0 ise, direkt "0" dondur.
    // (donguye girmeye gerek yok)
    if (value == 0)
        return "0";

    // Sayinin negatif olup olmadigini kontrol et
    bool negative = value < 0;

    // Negatifse, sayiyi pozitif yap.
    // Basamak ayirma islemini sadece pozitif sayilarla yapiyoruz.
    if (negative)
        value = -value;

    // Rakamlari biriktirmek icin string
    std::string result;

    // Sayi sifir olana kadar devam et
    while (value > 0) {

        // value % 10 :
        // Sayinin EN SAGDAKI basamagini verir
        // Ornek: 12345 % 10 = 5
        int digit = value % 10;

        // '0' + digit :
        // Sayisal rakami karaktere cevirir
        // Ornek: '0' + 5 -> '5'
        result.push_back('0' + digit);

        // value /= 10 :
        // Tam sayi bolmesi yapilir
        // Ondalik kisim atilir
        // Ornek: 12345 / 10 = 1234
        // Yani son basamak SILINMIS olur
        value /= 10;
    }

    // Eger sayi baslangicta negatifse,
    // en sona '-' karakterini ekle
    // (hala ters sirada oldugunu unutma)
    if (negative)
        result.push_back('-');

    // Rakamlar tersten eklendigi icin
    // string'i ters ceviriyoruz
    std::reverse(result.begin(), result.end());

    // Son haliyle string'i dondur
    return result;
}

