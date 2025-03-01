// Package config contains commands to talk to Redpanda's admin config
// endpoints.
//
// This package is named config to avoid import overlap with the rpk
// config package.
package config

import (
	"encoding/json"
	"fmt"

	"github.com/redpanda-data/redpanda/src/go/rpk/pkg/api/admin"
	"github.com/redpanda-data/redpanda/src/go/rpk/pkg/config"
	"github.com/redpanda-data/redpanda/src/go/rpk/pkg/out"
	"github.com/spf13/afero"
	"github.com/spf13/cobra"
)

// NewCommand returns the config admin command.
func NewCommand(fs afero.Fs) *cobra.Command {
	cmd := &cobra.Command{
		Use:   "config",
		Short: "View or modify Redpanda configuration through the admin listener.",
		Args:  cobra.ExactArgs(0),
	}
	cmd.AddCommand(
		newPrintCommand(fs),
		newLogLevelCommand(fs),
	)
	return cmd
}

func newPrintCommand(fs afero.Fs) *cobra.Command {
	var host string
	cmd := &cobra.Command{
		Use:     "print",
		Aliases: []string{"dump", "list", "ls", "display"},
		Short:   "Display the current Redpanda configuration.",
		Args:    cobra.ExactArgs(0),
		Run: func(cmd *cobra.Command, _ []string) {
			p := config.ParamsFromCommand(cmd)
			cfg, err := p.Load(fs)
			out.MaybeDie(err, "unable to load config: %v", err)

			cl, err := admin.NewHostClient(fs, cfg, host)
			out.MaybeDie(err, "unable to initialize admin client: %v", err)

			conf, err := cl.Config()
			out.MaybeDie(err, "unable to request configuration: %v", err)

			marshaled, err := json.MarshalIndent(conf, "", "  ")
			out.MaybeDie(err, "unable to json encode configuration: %v", err)

			fmt.Println(string(marshaled))
		},
	}

	cmd.Flags().StringVar(&host, "host", "", "either an index into admin_api hosts to issue the request to, or a hostname")
	cobra.MarkFlagRequired(cmd.Flags(), "host")

	return cmd
}

// rpk redpanda admin config log-level set
func newLogLevelCommand(fs afero.Fs) *cobra.Command {
	cmd := &cobra.Command{
		Use:   "log-level",
		Short: "Manage a broker's log level.",
		Args:  cobra.ExactArgs(0),
	}
	cmd.AddCommand(
		newLogLevelSetCommand(fs),
	)
	return cmd
}

func newLogLevelSetCommand(fs afero.Fs) *cobra.Command {
	var host string
	var level string
	var expirySeconds int

	cmd := &cobra.Command{
		Use:   "set [LOGGERS...]",
		Short: "Set broker logger's log level.",
		Long: `Set broker logger's log level.

This command temporarily changes a broker logger's log level. Each Redpanda
broker has many loggers, and each can be individually changed. Any change
to a logger persists for a limited amount of time, so as to ensure you do
not accidentally enable debug logging permanently.

It is optional to specify a logger; if you do not, this command will prompt
from the set of available loggers.

The special logger "all" enables all loggers. Alternatively, you can specify
many loggers at once. To see all possible loggers, run the following command:

  redpanda --help-loggers

This command accepts loggers that it does not know of to ensure you can
independently update your redpanda installations from rpk. The success or
failure of enabling each logger is individually printed.
`,

		Run: func(cmd *cobra.Command, loggers []string) {
			p := config.ParamsFromCommand(cmd)
			cfg, err := p.Load(fs)
			out.MaybeDie(err, "unable to load config: %v", err)

			cl, err := admin.NewHostClient(fs, cfg, host)
			out.MaybeDie(err, "unable to initialize admin client: %v", err)

			switch len(loggers) {
			case 0:
				choices := append([]string{"all"}, possibleLoggers...)
				pick, err := out.Pick(choices, "Which logger would you like to set (all selects everything)?")
				out.MaybeDie(err, "unable to pick logger: %v", err)
				if pick == "all" {
					loggers = possibleLoggers
				} else {
					loggers = []string{pick}
				}

			case 1:
				if loggers[0] == "all" {
					loggers = possibleLoggers
				}
			}

			type failure struct {
				logger string
				err    error
			}
			var failures []failure
			var successes []string

			for _, logger := range loggers {
				err := cl.SetLogLevel(logger, level, expirySeconds)
				if err != nil {
					failures = append(failures, failure{logger, err})
				} else {
					successes = append(successes, logger)
				}
			}

			if len(successes) > 0 {
				fmt.Println("SUCCESSES")
				for _, success := range successes {
					fmt.Println(success)
				}
				fmt.Println()
			}
			if len(failures) > 0 {
				fmt.Println("FAILURES")
				for _, failure := range failures {
					fmt.Printf("%s: %v\n", failure.logger, failure.err)
				}
				fmt.Println()
			}
		},
	}

	cmd.Flags().StringVarP(&level, "level", "l", "debug", "log level to set (error, warn, info, debug, trace)")
	cmd.Flags().IntVarP(&expirySeconds, "expiry-seconds", "e", 300, "seconds to persist this log level override before redpanda reverts to its previous settings (if 0, persist until shutdown)")

	cmd.Flags().StringVar(&host, "host", "", "either an index into admin_api hosts to issue the request to, or a hostname")
	cobra.MarkFlagRequired(cmd.Flags(), "host")

	return cmd
}

// List of possible loggers to set; more can be added in the future.
// To generate this list, run redpanda --help-loggers
var possibleLoggers = []string{
	"admin_api_server",
	"archival",
	"archival-ctrl",
	"assert",
	"cloud_storage",
	"cluster",
	"compaction_ctrl",
	"compression",
	"coproc",
	"dns_resolver",
	"exception",
	"fault_injector",
	"http",
	"httpd",
	"io",
	"json",
	"kafka",
	// "kafka/client", disabled until redpanda supports slashes in the handler
	"kvstore",
	"metrics-reporter",
	"offset_translator",
	"pandaproxy",
	// "r/heartbeat", disabled until redpanda supports slashes in the handler
	"raft",
	"redpanda::main",
	"rpc",
	"s3",
	"scheduler",
	"scollectd",
	"seastar",
	"security",
	"storage",
	"storage-gc",
	"syschecks",
	"tx",
}
