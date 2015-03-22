defmodule Elixirbot do
  use Application

  def start(_type, _args) do
    import Supervisor.Spec, warn: false

    # Bring up the network
    wireless_ethernet

    dispatch = :cowboy_router.compile([
      { :_,
        [
          {"/", :cowboy_static, {:priv_file, :elixirbot, "static/index.html"}},
          {"/static/[...]", :cowboy_static, {:priv_dir,  :elixirbot, "static"}},
          {"/video", VideoHandler, []},
          {"/websocket", WebsocketHandler, []}
      ]}
    ])
    { :ok, _ } = :cowboy.start_http(:http,
                                    100,
                                   [{:port, 80}],
                                   [{ :env, [{:dispatch, dispatch}]}]
                                   )

    children = [
      worker(I2c, ["i2c-1", 0x04, [name: BotI2c]]),
      worker(Camera, [[], [name: RaspiCamera]])
    ]

    opts = [strategy: :one_for_one, name: Elixirbot.Supervisor]
    Supervisor.start_link(children, opts)
  end

  def wired_ethernet() do
    :os.cmd('/sbin/ip link set eth0 up')
    :os.cmd('/sbin/ip addr add 192.168.1.40/24 dev eth0')
    :os.cmd('/sbin/ip route add default via 192.168.1.1')
  end

  def wireless_ethernet() do
    System.cmd("/usr/sbin/wpa_supplicant", ["-iwlan0", "-C/var/run/wpa_supplicant", "-B"])

    profile = %NetProfile{ifname: "wlan0",
                          ipv4_address_method: :dhcp,
                          wlan: %{ssid: "troodonsw", key_mgmt: :"WPA-PSK", psk: "ca6d7e0d08"
                          }}
    {:ok, nm} = NetManager.start_link
    {:ok, _sm} = WifiManager.start_link(nm, profile)
  end
end
