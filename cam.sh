
# Applies settings to digital camera.

base_url=http://1.1.1.1/cgi-bin
auth_url=?USER=Admin\&PWD=123456\&
encoder_url=${base_url}/encoder${auth_url}
system_url=${base_url}/system${auth_url}
channel_url=${encoder_url}CHANNEL=1\&

wget -q -O - "$@" "${system_url}RTSP_AUTHEN=1\&RTP_B2=2"

wget -q -O - "$@" "${channel_url}VIDEO_RESOLUTION=N640x480"

wget -q -O - "$@" "${channel_url}H264_PROFILE=HIGH"

wget -q -O - "$@" "${channel_url}VIDEO_H264_QUALITY=HIGH"


