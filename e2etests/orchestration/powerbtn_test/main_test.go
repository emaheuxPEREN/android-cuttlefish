// Copyright (C) 2025 The Android Open Source Project
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
	"bytes"
	"log"
	"strings"
	"testing"
	"time"

	"github.com/google/android-cuttlefish/e2etests/orchestration/common"

	hoclient "github.com/google/android-cuttlefish/frontend/src/libhoclient"
)

const baseURL = "http://0.0.0.0:2080"

func TestPowerBtn(t *testing.T) {
	adbH := common.NewAdbHelper()
	if err := adbH.StartServer(); err != nil {
		t.Fatal(err)
	}
	srv := hoclient.NewHostOrchestratorClient(baseURL)
	t.Cleanup(func() {
		if err := common.CollectHOLogs(baseURL); err != nil {
			log.Printf("failed to collect HO logs: %s", err)
		}
	})
	imageDir, err := common.PrepareArtifact(srv, "../artifacts/images.zip")
	if err != nil {
		t.Fatal(err)
	}
	hostPkgDir, err := common.PrepareArtifact(srv, "../artifacts/cvd-host_package.tar.gz")
	if err != nil {
		t.Fatal(err)
	}
	cvd, err := common.CreateCVDFromImageDirs(srv, hostPkgDir, imageDir)
	if err != nil {
		t.Fatal(err)
	}
	// Monitor input devices events
	if err := adbH.Connect(cvd.ADBSerial); err != nil {
		t.Fatal(err)
	}
	// Power button should generate at least 4 events.
	// EV_KEY       KEY_POWER            DOWN
	// EV_SYN       SYN_REPORT           00000000
	// EV_KEY       KEY_POWER            UP
	// EV_SYN       SYN_REPORT           00000000
	cmd := adbH.BuildShellCommand(cvd.ADBSerial, []string{"getevent", "-l", "-c 2", "/dev/input/event0"})
	stdoutBuff := &bytes.Buffer{}
	cmd.Stdout = stdoutBuff
	if err := adbH.WaitForDevice(cvd.ADBSerial); err != nil {
		t.Fatal(err)
	}
	if err := cmd.Start(); err != nil {
		t.Fatal(err)
	}
	// Add some delay for `getevent` to properly start listening.
	time.Sleep(5 * time.Second)

	if err := srv.Powerbtn(cvd.Group, cvd.Name); err != nil {
		t.Fatal(err)
	}

	if err := cmd.Wait(); err != nil {
		t.Fatal(err)
	}
	stdoutStr := stdoutBuff.String()
	log.Printf("getevent stdout: %s", stdoutStr)
	const eventName = "KEY_POWER"
	if !strings.Contains(stdoutStr, eventName) {
		t.Errorf("event %q was not captured", eventName)
	}
}
