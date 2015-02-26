defmodule Elixirbot do
  use Application

  def start(_type, _args) do
    import Supervisor.Spec, warn: false

    # Bring up the network (FIXME)
    #:os.cmd('/sbin/ip link set eth0 up')
    #:os.cmd('/sbin/ip addr add 192.168.1.40/24 dev eth0')
    #:os.cmd('/sbin/ip route add default via 192.168.1.1')
    System.cmd("/usr/sbin/wpa_supplicant", ["-iwlan0", "-C/var/run/wpa_supplicant", "-B"])

    profile = %NetProfile{ifname: "wlan0",
                          ipv4_address_method: :dhcp,
                          wlan: %{ssid: "hunleth", key_mgmt: :"WPA-PSK", psk: "ahqwlhvjgxsltfmy"}}

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
      worker(Camera, [[], [name: RaspiCamera]])
    ]

    opts = [strategy: :one_for_one, name: Elixirbot.Supervisor]
    Supervisor.start_link(children, opts)
  end
end
