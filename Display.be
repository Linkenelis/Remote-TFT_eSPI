var displayIp

class udp_listener
  var u
  def init(ip, port)
    self.u = udp()
    print(self.u.begin(ip, port))
    tasmota.add_driver(self)
  end
  def every_50ms()
    import string
    var packet = self.u.read()
    while packet != nil
      tasmota.log(string.format(">>> Received packet ([%s]:%i): %s", self.u.remote_ip, self.u.remote_port, packet.tohex()), 2)
      displayIp = self.u.remote_ip
      print ("displayIp")
      print (displayIp)      
      packet = self.u.read()
    end
  end
end

#timer
def set_timer_every(delay,f)
    var now=tasmota.millis()
    tasmota.set_timer((now+delay/4+delay)/delay*delay-now, def() set_timer_every(delay,f) f() end)
end

#cannot start UDP without WiFi, so check year > 2020
def check_wifi()
    if number(tasmota.time_dump(tasmota.rtc()['local'])['year'])<2020
        tasmota.set_timer(5000, check_wifi) #try again in 5 secs
        print("No WiFi")
    else
        udp_listener("",2000)
        print("Yes! WiFi")
    end
end
tasmota.set_timer(5000, check_wifi)

def SendToDisplay()
    import json
    var u=udp()
    var msg="tft.drawString("
    msg +=json.load(tasmota.read_sensors())['ENERGY']['TotalStartTime']
    msg +=",5,310)"
    u.send(displayIp, 2000, bytes().fromstring(msg))
end

set_timer_every(60000, SendToDisplay)