defmodule WebsocketHandler do
  @behaviour :cowboy_websocket_handler
  defstruct left_polarity: 1,
            right_polarity: 1,
            is_stopped: true

  def init({:tcp, :http}, _req, _opts) do
    {:upgrade, :protocol, :cowboy_websocket}
  end

  def websocket_init(_TransportName, req, _opts) do
    {left, right} = read_polarity
    # poll voltage every second
    :erlang.start_timer(1000, self(), [])
    {:ok, req, %WebsocketHandler{left_polarity: left, right_polarity: right}}
  end

  def websocket_terminate(_reason, _req, state) do
    # stop the motors - it's funny when you don't do this.
    stop(state)
    :ok
  end

  def websocket_handle({:text, content}, req, state) do
    { :ok, msg } = JSX.decode(content)

    change_motors(msg["distance"], msg["angle"], state)

    # Hack
    state = %{state | is_stopped: msg["distance"] < 10}

    { :ok, reply } = JSX.encode(%{ reply: :ok})

    {:reply, {:text, reply}, req, state}
  end

  def websocket_handle(_data, req, state) do
    {:ok, req, state}
  end

  def websocket_info({:timeout, _ref, _foo}, req, state) do
    { :ok, message } = JSX.encode(%{ volts: volts})

    # Hack to work around I2C errors
    if state == :stopped, do: stop(state)

    :erlang.start_timer(1000, self(), [])
    { :reply, {:text, message}, req, state}
  end

  # fallback message handler
  def websocket_info(_info, req, state) do
    {:ok, req, state}
  end

  defp change_motors(distance, _angle, state) when distance < 10, do: stop(state)
  defp change_motors(_distance, angle, state) when angle < 45 or angle >= 315, do: forward(state)
  defp change_motors(_distance, angle, state) when angle < 135 and angle >= 45, do: right(state)
  defp change_motors(_distance, angle, state) when angle < 225 and angle >= 135, do: reverse(state)
  defp change_motors(_distance, angle, state) when angle < 315 and angle >= 225, do: left(state)

  # Pulse duration to motor direction
  defp ccw(polarity) when polarity >= 0, do: 1800
  defp ccw(_polarity), do: 1200
  defp cw(polarity) when polarity >= 0, do: 1200
  defp cw(_polarity), do: 1800
  defp stopped(_polarity), do: 1500

  defp stop(state), do: motors(state, &stopped/1, &stopped/1)
  defp forward(state), do: motors(state, &cw/1, &ccw/1)
  defp reverse(state), do: motors(state, &ccw/1, &cw/1)
  defp left(state), do: motors(state, &ccw/1, &ccw/1)
  defp right(state), do: motors(state, &cw/1, &cw/1)

  # I2C registers
  #  0 - left LSB
  #  1 - left MSB
  #  2 - right LSB
  #  3 - right MSB
  #  4 - battery millivolts LSB (read-only)
  #  5 - battery millivolts MSB (read-only)

  defp motors(state, left, right) do
    left_duration = left.(state.left_polarity)
    right_duration = right.(state.right_polarity)

    I2c.write(BotI2c, <<0, left_duration::little-size(16), right_duration::little-size(16)>>)
  end

  defp volts() do
    I2c.write_read(BotI2c, <<4>>, 2)
      |> decode_volts
  end

  defp decode_volts(<<millivolts::little-size(16)>>) when millivolts > 0 and millivolts < 10000 do
    millivolts / 1000
  end
  defp decode_volts(_), do: 0

  # Read the motor polarity from a file since I bought cheap motors. :(
  # Return {left, right} were x=1 for a normal servo and x=-1 for a reversed one
  defp read_polarity() do
    case File.read("/root/polarity.json") do
      {:ok, contents} ->
        {:ok, parsed} = JSX.decode(contents)
        IO.write("Read servo polarity: #{contents}\n")
        {parsed["left"], parsed["right"]}
      _ ->
        {1, 1}
    end
  end

end

