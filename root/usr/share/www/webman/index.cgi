#!/bin/sh

PATH="$PATH:/bin"
PATH="$PATH:/sbin"

if [ -f /etc/sysconfig/network ]; then
  . /etc/sysconfig/network
  CONFIGFILE=/data/network-config
else
  DHCP=yes
  IPADDR=192.168.168.2
  NETMASK=255.255.255.0
  GATEWAY=192.168.168.0
  DEFAULT_GATEWAY=192.168.168.1
  CONFIGFILE=/tmp/aaa
fi
if [ -f /data/network-config ]; then
  . /data/network-config
else
  IPADDR=192.168.168.2
  NETMASK=255.255.255.0
  GATEWAY=192.168.168.0
  DEFAULT_GATEWAY=192.168.168.1
fi

eval $(echo "$QUERY_STRING"|awk -F'&' '{for(i=1;i<=NF;i++){print $i}}')
_BTN1=`httpd -d $btn_update`
_BTN2=`httpd -d $btn_update_reboot`
_RDOACTION=`httpd -d $a`
_RDOIP=`httpd -d $rdo_ip`
_IPADDR=`httpd -d $ipaddr`
_NETMASK=`httpd -d $netmask`
_DEFAULT_GATEWAY=`httpd -d $gateway`

if [ "$_RDOACTION" = "dhcp" ]; then
    DHCP="yes"
elif [ "$_RDOACTION" = "manual" ]; then
    DHCP="no"
else
    if [ "$_RDOIP" = "dhcp" ]; then
        DHCP="yes"
    elif [ "$_RDOIP" = "manual" ]; then
        DHCP="no"
    fi
fi

if [ ! -z "$_BTN1" -o ! -z "$_BTN2" ]; then
    if [ ! -z "$_IPADDR" -a "$_IPADDR" != "$IPADDR" ]; then
        IPADDR=$_IPADDR
    fi
    if [ ! -z "$_NETMASK" -a "$_NETMASK" != "$NETMASK" ]; then
        NETMASK=$_NETMASK
    fi
    if [ ! -z "$_DEFAULT_GATEWAY" -a "$_DEFAULT_GATEWAY" != "$DEFAULT_GATEWAY" ]; then
        DEFAULT_GATEWAY=$_DEFAULT_GATEWAY
    fi
    echo "DHCP=$DHCP">$CONFIGFILE
    echo "IPADDR=$IPADDR">>$CONFIGFILE
    echo "NETMASK=$NETMASK">>$CONFIGFILE
    echo "DEFAULT_GATEWAY=$DEFAULT_GATEWAY">>$CONFIGFILE
    /bin/sync

    echo "Expires: Mon, 26 Jul 1990 05:00:00 GMT
    Cache-Control: no-store, no-cache, must-revalidate
    Pragma: no-cache
    Content-type: text/html

    <html>
    <head>
    <title>Redirect ...</title>
    <meta http-equiv=\"REFRESH\" content=\"0;url=/\">
    <meta http-equiv=\"cache-control\" content=\"no-cache\">
    <meta http-equiv=\"pragma\" content=\"no-cache\">
    <meta http-equiv=\"expires\" content=\"0\">
    </head>
    <body>
    </body>
    </html>
    "

    if [ ! -z "$_BTN2" ]; then
        /sbin/reboot
    fi

    return
fi

if [ "$DHCP" = "yes" ]; then
    FORM_DATA='
        <form method="get" onSubmit="return checkform()" class="basic-grey">
            <h1>设备管理&nbsp;&nbsp;&nbsp;&nbsp;
            <span> &nbsp </span>
            </h1>
            <label>
            <span>IP设置：</span>
            <input type="radio" name="rdo_ip" id="rdo_ip" value="dhcp" onclick="to_change()" checked /> DHCP自动获取
            &nbsp &nbsp &nbsp &nbsp &nbsp &nbsp
            <input type="radio" name="rdo_ip" id="rdo_ip" value="manual" onclick="to_change()" /> 手动设置
            &nbsp &nbsp &nbsp &nbsp &nbsp &nbsp
            </label> <br>
            <input type="submit" name="btn_update" class="button" value="更新" />
            <span>&nbsp;</span>
            <input type="submit" name="btn_update_reboot" class="button" value="更新并重启" />
            <span>&nbsp;</span>
            <input type="button" name="btn_reset" class="button" value="重填" onclick="to_reset()" />
        </form>
    '
else
    FORM_DATA='
        <form method="get" onSubmit="return checkform()" class="basic-grey">
            <h1>设备管理&nbsp;&nbsp;&nbsp;&nbsp;
            <span> &nbsp </span>
            </h1>
            <label>
            <span>IP设置：</span>
            <input type="radio" name="rdo_ip" id="rdo_ip" value="dhcp" onclick="to_change()" /> DHCP自动获取
            &nbsp &nbsp &nbsp &nbsp &nbsp &nbsp
            <input type="radio" name="rdo_ip" id="rdo_ip" value="manual" onclick="to_change()" checked /> 手动设置
            &nbsp &nbsp &nbsp &nbsp &nbsp &nbsp
            </label> <br>
            <label> <span>IP地址：</span>
            <input type="text" name="ipaddr" id="ipaddr" value="'$IPADDR'" />
            </label>
            <label> <span>IP掩码：</span>
            <input type="text" name="netmask" id="netmask" value="'$NETMASK'" />
            </label>
            <label> <span>IP网关：</span>
            <input type="text" name="gateway" id="gateway" value="'$DEFAULT_GATEWAY'" />
            </label>
            <input type="submit" name="btn_update" class="button" value="更新" />
            <span>&nbsp;</span>
            <input type="submit" name="btn_update_reboot" class="button" value="更新并重启" />
            <span>&nbsp;</span>
            <input type="button" name="btn_reset" class="button" value="重填" onclick="to_reset()" />
        </form>
    '
fi

cat << EOF
Expires: Mon, 26 Jul 1990 05:00:00 GMT
Cache-Control: no-store, no-cache, must-revalidate
Pragma: no-cache
Content-type: text/html

<html>
	<head>
		<title>设备管理</title>
		<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
		<meta http-equiv="X-UA-Compatible" content="IE=11" />
		<meta http-equiv="cache-control" content="no-cache, no-store, must-revalidate">
		<meta http-equiv="pragma" content="no-cache">
		<meta http-equiv="expires" content="0">
		<meta name="viewport" content="initial-scale=1.0; maximum-scale=1.0;"/>

		<link rel="short icon" type="image/png" href="resources/icon16.png" />
		<link rel="icon" type="image/png" href="resources/icon16.png" />
        <script type="text/javascript">
            function checkform()
            {
                var ipaddr=document.getElementById("ipaddr");
                var netmask=document.getElementById("netmask");
                var gateway=document.getElementById("gateway");
                var regIP=/^(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$/;
                if (ipaddr != null) {
                    if(ipaddr.value==""){
                        alert("IP地址不能为空，请输入！");
                        ipaddr.focus();
                        return false;
                    } else if (regIP.test(ipaddr.value)!=true) {
                        alert("IP地址有误，请重新输入！");
                        ipaddr.focus();
                        return false;
                    } else if (netmask.value=="") {
                        alert("IP掩码不能为空，请输入！");
                        netmask.focus();
                        return false;
                    } else if (regIP.test(netmask.value)!=true) {
                        alert("IP掩码有误，请重新输入！");
                        netmask.focus();
                        return false;
                    } else if (gateway.value=="") {
                        alert("IP掩码不能为空，请输入！");
                        gateway.focus();
                        return false;
                    } else if (regIP.test(gateway.value)!=true) {
                        alert("IP掩码有误，请重新输入！");
                        gateway.focus();
                        return false;
                    }
                    if (ipaddr)
                    {
                        return confirm("您选择的IP模式为：手动设置"
                                +"\n"+"您输入的IP地址为："+ipaddr.value
                                +"\n"+"您输入的IP掩码为："+netmask.value
                                +"\n"+"您输入的IP网关为："+gateway.value
                                +"\n"+"\n"+"确定继续?");
                    }
                }
                return confirm("您选择的IP模式为：自动获取"
                        +"\n"+"\n"+"确定继续?");
            }
            function to_change()
            {
                var rdoip=document.getElementsByName("rdo_ip");
                for (var i=0;i<rdoip.length;i++)
                {
                    if (rdoip[i].checked == true)
                    {
                        if (rdoip[i].value == "dhcp")
                        {
                            location.href='/?a=dhcp';
                        } else if (rdoip[i].value == "manual") {
                            location.href='/?a=manual';
                        }
                    }
                }
            }
            function to_reset()
            {
                location.href='/';
            }
        </script>
        <link rel="stylesheet" type="text/css" href="style.css" />
	</head>
	<body>
		<img src="resources/bg_02.png" width=100% height=100% style="position: absolute; top: 0; left: 0; z-index: -1000" />
        <center>
            $FORM_DATA
        </center>
	</body>
</html>

EOF
