xset -dpms
xset s off

cat /etc/em.conf | sed 's/=\(.*\)/="\1"/g' > /tmp/.em.conf
source /tmp/.em.conf && rm -f /tmp/.em.conf

xrandr | grep '*' > ${EM_RESOLUTION}

while true; do
	/opt/firefox/firefox http://localhost:8081/em/
done
