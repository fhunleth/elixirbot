defmodule Elixirbot.Mixfile do
  use Mix.Project

  def project do
    [app: :elixirbot,
     version: "0.0.1",
     elixir: "~> 1.0.0",
     deps: deps]
  end

  # Configuration for the OTP application
  #
  # Type `mix help compile.app` for more information
  def application do
    [
      mod: { Elixirbot, [] },
      applications: [:exjsx, :cowboy, :ranch, :elixir_ale, :net_managers]
    ]
  end

  # Dependencies can be Hex packages:
  #
  #   {:mydep, "~> 0.3.0"}
  #
  # Or git/path repositories:
  #
  #   {:mydep, git: "https://github.com/elixir-lang/mydep.git", tag: "0.1.0"}
  #
  # Type `mix help deps` for more examples and options
  defp deps do
    [ { :cowboy, "1.0.0" },
      { :exjsx, "~> 3.1.0" },
      { :exrm, "~> 0.15.0"},
      { :elixir_ale, github: "fhunleth/elixir_ale", tag: "master"},
      { :net_managers, github: "fhunleth/net_managers.ex", tag: "master" }
    ]
  end
end
