defmodule VideoHandler do

  def init(_type, req, []) do
    {:ok, req, :no_state}
  end


  def handle(request, _state) do
    { :ok, request } = :cowboy_req.chunked_reply(
      200,
      [ {"connection", "close"},
        {"Cache-Control", "no-cache"},
        {"MIME-Version", "1.0"},
        {"content-type", "multipart/x-mixed-replace;boundary=#{boundary}"} ],
      request
    )

    # Send the first delimiter
    :ok = :cowboy_req.chunk([delimiter], request)

    send_pictures(request)
  end

  def terminate(_reason, _request, _state) do
    :ok
  end

  defp boundary() do
    "foofoo"
  end
  defp delimiter() do
    "\r\n--#{boundary}\r\n"
  end
  defp multipart_header(content) do
    "Content-Type: image/jpeg\r\nContent-Length: #{byte_size(content)}\r\n\r\n"
  end
  defp send_pictures(request) do
    jpg = Camera.next_picture(RaspiCamera)
    msg = [multipart_header(jpg), jpg, delimiter]
    :ok = :cowboy_req.chunk(msg, request)
    send_pictures(request)
  end
end
