// Copyright (C) 2024 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package main

import (
	"log"
	"os"
	"testing"

	"github.com/google/android-cuttlefish/e2etests/orchestration/common"

	hoapi "github.com/google/android-cuttlefish/frontend/src/host_orchestrator/api/v1"
	hoclient "github.com/google/android-cuttlefish/frontend/src/libhoclient"
	"github.com/google/go-cmp/cmp"
)

const baseURL = "http://0.0.0.0:2080"

func TestCreateSingleInstance(t *testing.T) {
	srv := hoclient.NewHostOrchestratorClient(baseURL)
	t.Cleanup(func() {
		if err := common.CollectHOLogs(baseURL); err != nil {
			log.Printf("failed to collect HO logs: %s", err)
		}
	})
	buildID := os.Getenv("BUILD_ID")
	buildTarget := os.Getenv("BUILD_TARGET")
	fetchReq := &hoapi.FetchArtifactsRequest{
		AndroidCIBundle: &hoapi.AndroidCIBundle{
			Build: &hoapi.AndroidCIBuild{
				BuildID: buildID,
				Target:  buildTarget,
			},
			Type: hoapi.MainBundleType,
		},
	}
	_, err := srv.FetchArtifacts(fetchReq, &hoclient.AccessTokenBuildAPICreds{})
	if err != nil {
		t.Fatal(err)
	}
	createReq := &hoapi.CreateCVDRequest{
		CVD: &hoapi.CVD{
			BuildSource: &hoapi.BuildSource{
				AndroidCIBuildSource: &hoapi.AndroidCIBuildSource{
					MainBuild: &hoapi.AndroidCIBuild{
						BuildID: buildID,
						Target:  buildTarget,
					},
				},
			},
		},
	}

	got, createErr := srv.CreateCVD(createReq, &hoclient.AccessTokenBuildAPICreds{})

	if err := common.DownloadHostBugReport(srv, "cvd"); err != nil {
		t.Errorf("failed creating bugreport: %s\n", err)
	}
	if createErr != nil {
		t.Fatal(createErr)
	}
	if err := common.VerifyLogsEndpoint(baseURL, "cvd", "1"); err != nil {
		t.Fatalf("failed verifying /logs endpoint: %s", err)
	}
	want := &hoapi.CreateCVDResponse{
		CVDs: []*hoapi.CVD{
			{
				Group:          "cvd",
				Name:           "1",
				BuildSource:    &hoapi.BuildSource{},
				Status:         "Running",
				Displays:       []string{"720 x 1280 ( 320 )"},
				WebRTCDeviceID: "cvd-1-1",
				ADBSerial:      "0.0.0.0:6520",
			},
		},
	}
	if diff := cmp.Diff(want, got); diff != "" {
		t.Errorf("response mismatch (-want +got):\n%s", diff)
	}
}
