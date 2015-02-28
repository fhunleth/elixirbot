defmodule WebsocketHandler do
  @behaviour :cowboy_websocket_handler

  def init({:tcp, :http}, _req, _opts) do
    {:upgrade, :protocol, :cowboy_websocket}
  end

  def websocket_init(_TransportName, req, _opts) do
    IO.puts "init.  Starting timer. PID is #{inspect(self())}"

    # TODO
    :erlang.start_timer(1000, self(), [])
    {:ok, req, :undefined_state }
  end

  def websocket_terminate(_reason, _req, _state) do
    # stop the motors - it's funny when you don't do this.
    I2c.write(BotI2c, <<0, 22, 22>>)
    :ok
  end

  def websocket_handle({:text, content}, req, state) do
    { :ok, msg } = JSX.decode(content)

    result = change_motors(msg["distance"], msg["angle"])

    { :ok, reply } = JSX.encode(%{ reply: result})

    {:reply, {:text, reply}, req, state}
  end

  def websocket_handle(_data, req, state) do
    {:ok, req, state}
  end

  def websocket_info({:timeout, _ref, _foo}, req, state) do
    # TODO
    time = time_as_string()
    { :ok, message } = JSEX.encode(%{ time: time})
    :erlang.start_timer(1000, self(), [])

    { :reply, {:text, message}, req, state}
  end

  # fallback message handler
  def websocket_info(_info, req, state) do
    {:ok, req, state}
  end

  def time_as_string do
    {hh,mm,ss} = :erlang.time()
    :io_lib.format("~2.10.0B:~2.10.0B:~2.10.0B",[hh,mm,ss])
      |> :erlang.list_to_binary()
  end

  defp change_motors(distance, _angle) when distance < 10 do
    I2c.write(BotI2c, <<0, 22, 22>>)
  end
  # forward
  defp change_motors(_distance, angle) when angle < 45 or angle >= 315 do
    I2c.write(BotI2c, <<0, 28, 28>>)
  end
  # right
  defp change_motors(_distance, angle) when angle < 135 and angle >= 45 do
    I2c.write(BotI2c, <<0, 28, 17>>)
  end
  # backward
  defp change_motors(_distance, angle) when angle < 225 and angle >= 135 do
    I2c.write(BotI2c, <<0, 17, 17>>)
  end
  # left
  defp change_motors(_distance, angle) when angle < 315 and angle >= 225 do
    I2c.write(BotI2c, <<0, 17, 28>>)
  end
end

