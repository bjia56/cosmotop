package main

import (
	"testing"
)

func TestClassifyLaunchMode(t *testing.T) {
	t.Parallel()

	tests := []struct {
		name string
		args []string
		want launchMode
	}{
		{name: "no args launches desktop", args: nil, want: launchModeDesktop},
		{name: "help passthrough", args: []string{"--help"}, want: launchModePassthrough},
		{name: "version passthrough", args: []string{"--version"}, want: launchModePassthrough},
		{name: "show defaults passthrough", args: []string{"--show-defaults"}, want: launchModePassthrough},
		{name: "show themes passthrough", args: []string{"--show-themes"}, want: launchModePassthrough},
		{name: "licenses passthrough", args: []string{"--licenses"}, want: launchModePassthrough},
		{name: "mcp passthrough", args: []string{"--mcp"}, want: launchModePassthrough},
		{name: "valid tui flags launch desktop", args: []string{"--debug", "-p", "1", "-u", "500", "-o", "theme=Default"}, want: launchModeDesktop},
		{name: "missing preset value passthrough", args: []string{"-p"}, want: launchModePassthrough},
		{name: "unknown arg passthrough", args: []string{"--no-such-arg"}, want: launchModePassthrough},
	}

	for _, tt := range tests {
		tt := tt
		t.Run(tt.name, func(t *testing.T) {
			t.Parallel()
			if got := classifyLaunchMode(tt.args); got != tt.want {
				t.Fatalf("classifyLaunchMode(%v) = %v, want %v", tt.args, got, tt.want)
			}
		})
	}
}
