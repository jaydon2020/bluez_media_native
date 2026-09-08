import dbus, dbus.service, dbus.mainloop.glib
from gi.repository import GLib
import os

dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
bus = dbus.SessionBus()
name = dbus.service.BusName('org.bluez', bus)
obex_name = dbus.service.BusName('org.bluez.obex', bus)
iface = 'org.bluez.MediaPlayer1'
status = 'paused'
present = True
registrations = {}
class Player(dbus.service.Object):
    @dbus.service.signal('org.freedesktop.DBus.Properties', signature='sa{sv}as')
    def PropertiesChanged(self, interface, changed, invalidated): pass
    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='s', out_signature='a{sv}')
    def GetAll(self, interface):
        return {'Status': status, 'Device': dbus.ObjectPath('/device'),
                'ObexPort': dbus.UInt16(4097),
                'Track': dbus.Dictionary({'ImgHandle': 'image'}, signature='sv')}

    @dbus.service.method(iface, in_signature='', out_signature='', async_callbacks=('reply','error'))
    def Play(self, reply, error):
        def change():
            global status
            status = 'stopped'
            self.PropertiesChanged(iface, {'Status': status}, [])
            return False
        GLib.timeout_add(100, change)
        GLib.timeout_add(800, lambda: (reply(), False)[1])
class Root(dbus.service.Object):
    @dbus.service.method('org.bluez.Media1', in_signature='oa{sv}')
    def RegisterPlayer(self, path, properties):
        required = {'Identity', 'PlaybackStatus', 'Position', 'Metadata',
                    'LoopStatus', 'Shuffle', 'CanPlay', 'CanControl'}
        if not required.issubset(properties):
            raise dbus.exceptions.DBusException('Missing MPRIS properties',
                name='org.bluez.Error.InvalidArguments')
        if properties['CanPlay'] or properties['CanControl']:
            raise dbus.exceptions.DBusException('Inert player advertised playback')
        registrations[str(path)] = properties
    @dbus.service.method('org.bluez.Media1', in_signature='o')
    def UnregisterPlayer(self, path): registrations.pop(str(path), None)

    @dbus.service.method('org.freedesktop.DBus.ObjectManager', out_signature='a{oa{sa{sv}}}')
    def GetManagedObjects(self):
        global status
        if not present: return {}
        snapshot = {'/player': {iface: {'Status': status}},
                    '/session': {'org.bluez.obex.Session1': {
                        'Destination': '00:11:22:33:44:55', 'PSM': dbus.UInt16(4097)}}}

        status = 'playing'
        player.PropertiesChanged(iface, {'Status': status}, [])
        return snapshot
    @dbus.service.signal('org.freedesktop.DBus.ObjectManager', signature='oas')
    def InterfacesRemoved(self, path, interfaces): pass
    @dbus.service.method('review.Test', in_signature='s')
    def Step(self, command):
        global present, status
        if command == 'reset':
            present = True
            status = 'paused'
        elif command == 'assert_cancelled':
            if transfer.cancelled == 0: raise RuntimeError('Transfer was not cancelled')
        elif command == 'invalidate': player.PropertiesChanged(iface, {}, ['Status'])
        elif command == 'restart':
            present = False
            bus.release_name('org.bluez')
            GLib.timeout_add(50, lambda: (bus.request_name('org.bluez'), False)[1])

class MediaTransport(dbus.service.Object):
    @dbus.service.method('org.bluez.MediaTransport1', out_signature='hqq')
    def Acquire(self):
        read_fd, write_fd = os.pipe()
        try:
            return dbus.types.UnixFd(read_fd), dbus.UInt16(672), dbus.UInt16(672)
        finally:
            os.close(read_fd)
            os.close(write_fd)
    @dbus.service.method('org.bluez.MediaTransport1')
    def Release(self): pass

media_transport = MediaTransport(bus, '/transport')

class Device(dbus.service.Object):
    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='s', out_signature='a{sv}')
    def GetAll(self, interface): return {'Address': '00:11:22:33:44:55'}

class Image(dbus.service.Object):
    @dbus.service.method('org.bluez.obex.Image1', in_signature='ss', out_signature='oa{sv}')
    def GetThumbnail(self, target, handle):
        with open(target, 'wb') as file: file.write(b'partial')
        transfer.cancelled = 0
        return '/transfer', {'Size': dbus.UInt64(100)}

class Transfer(dbus.service.Object):
    cancelled = 0
    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='ss', out_signature='v')
    def Get(self, interface, prop): return 'active'
    @dbus.service.method('org.bluez.obex.Transfer1')
    def Cancel(self): self.cancelled += 1

device = Device(bus, '/device')
image = Image(bus, '/session')
transfer = Transfer(bus, '/transfer')
root = Root(bus, '/')
player = Player(bus, '/player')
open(os.environ['BLUEZ_TEST_READY'], 'w').close()
GLib.MainLoop().run()
