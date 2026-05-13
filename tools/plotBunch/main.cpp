#include <TFile.h>
#include <TTree.h>
#include <TH1D.h>
#include <TCanvas.h>
#include <TPad.h>
#include <TRootCanvas.h>
#include <TApplication.h>

#include <algorithm>
#include <iostream>
#include <cmath>
#include <string>
#include "RootTreeReader.h"
#include "ANNIEEventReader.h"
#include "TankClusterReader.h"

void PlotBunch(const char* fileName);

int main(int argc, char** argv) {

    const char* fileName = "ANNIETree.root";

    if (argc > 1) {
        fileName = argv[1];
    }

    PlotBunch(fileName);

    return 0;
}

void PlotBunch(const char* fileName) {

    TApplication* app = new TApplication("Root Application", nullptr, nullptr);

    RootTreeReader treeReader(fileName, "Event");

    if (!treeReader.IsValid())
        return;

    TTree* tree = treeReader.GetTree();

    ANNIEEventReader eventReader;
    eventReader.Init(tree);

    
    TankClusterReader tankClusterReader;
    tankClusterReader.Init(tree);

    Long64_t nEntries = tree->GetEntries();
    std::cout << "Number of events in this tree: " << nEntries << std::endl;

    double BRFFirstPeakFit;
    tree->SetBranchAddress("BRFFirstPeakFit", &BRFFirstPeakFit);

    // 2 ns bin width: 0–2000 ns
    TH1D* hBestClusterTime = new TH1D(
        "hBestClusterTime",
        "Best cluster time - BRF;Time [ns];Counts / 2 ns",
        1000, 0, 2000
    );

    TH1D* hEarliestHitTime = new TH1D(
        "hEarliestHitTime",
        "Earliest hit time in best cluster - BRF;Time [ns];Counts / 2 ns",
        1000, 0, 2000
    );

    double clusterTimeMin = 0.0;
    double clusterTimeMax = 2000.0;

    for (Long64_t i = 0; i < nEntries; ++i) {

        tree->GetEntry(i);

        ETankClusters eTankClusters;
        tankClusterReader.ReadEntry(i, eTankClusters);

        double t0 = BRFFirstPeakFit * 0.001; // ps -> ns varsayımı

        double maxPENumber = -1.0;
        const TankCluster* bestCluster = nullptr;

        // En yüksek PE'ye sahip prompt cluster'ı bul
        for (const TankCluster& cl : eTankClusters.Get()) {

            double clusterTime = cl.GetClusterTime();

            if (clusterTime > clusterTimeMin && clusterTime < clusterTimeMax) {

                if (cl.GetMaxPE() > maxPENumber) {
                    maxPENumber = cl.GetMaxPE();
                    bestCluster = &cl;
                }
            }
        }

        // Eğer uygun cluster yoksa geç
        if (bestCluster == nullptr) {
            eTankClusters.Clear();
            continue;
        }

        // 1) Best cluster time
        double bestClusterTime = bestCluster->GetClusterTime();
        hBestClusterTime->Fill(bestClusterTime - t0);

        // 2) Best cluster içindeki en erken hit zamanı
        double earliestHitTime = 1e9;

        for (const PMTHit& hit : bestCluster->GetHits()) {
            if (hit.GetT() < earliestHitTime) {
                earliestHitTime = hit.GetT();
            }
        }

        if (earliestHitTime < 1e9) {
            hEarliestHitTime->Fill(earliestHitTime - t0);
        }

        eTankClusters.Clear();
    }


TCanvas* c1 = new TCanvas("c1", "BNB bunch structure", 1200, 800);

const char* pdfName = "BunchPlots.pdf";

// PDF aç
c1->Print((std::string(pdfName) + "[").c_str());


// -------------------------
// 1. sayfa: best cluster time
// -------------------------
c1->Clear();

hBestClusterTime->SetLineColor(kRed);
hBestClusterTime->SetLineWidth(2);
hBestClusterTime->Draw("hist");

c1->Modified();
c1->Update();
c1->Print(pdfName);


// -------------------------
// 2. sayfa: earliest hit time + ZOOM
// -------------------------
c1->Clear();

hEarliestHitTime->SetLineColor(kBlue);
hEarliestHitTime->SetLineWidth(2);
hEarliestHitTime->Draw("hist");


// -------------------------
// ZOOM PAD (inset)
// -------------------------
TPad* zoomPad = new TPad("zoomPad", "zoomPad",
                         0.20, 0.55,   // sol-alt
                         0.60, 0.90);  // sağ-üst

zoomPad->SetFillColor(0);
zoomPad->SetBorderMode(1);
zoomPad->Draw();
zoomPad->cd();

TH1D* hZoom = (TH1D*)hEarliestHitTime->Clone("hZoom");

// 🔴 BURAYI İSTEDİĞİN GİBİ AYARLA
hZoom->GetXaxis()->SetRangeUser(940, 1050);

hZoom->SetTitle("");
hZoom->SetStats(0);

// küçük pad için axis ayarları
hZoom->GetXaxis()->SetLabelSize(0.08);
hZoom->GetYaxis()->SetLabelSize(0.08);
hZoom->GetXaxis()->SetTitleSize(0.08);
hZoom->GetYaxis()->SetTitleSize(0.08);

hZoom->Draw("hist");


// tekrar ana canvas'a dön
c1->cd();

c1->Modified();
c1->Update();
c1->Print(pdfName);


// PDF kapat
c1->Print((std::string(pdfName) + "]").c_str());



    TRootCanvas* rc = (TRootCanvas*)c1->GetCanvasImp();
    rc->Connect("CloseWindow()", "TApplication", gApplication, "Terminate()");

    app->Run();
}
