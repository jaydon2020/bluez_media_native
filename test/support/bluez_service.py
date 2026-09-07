import dbus, dbus.service, dbus.mainloop.glib
from gi.repository import GLib
import os

dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
bus = dbus.SessionBus()
name = dbus.service.BusName('org.bluez', bus)
iface = 'org.bluez.MediaPlayer1'
status = 'paused'
present = True
registrations = {}
class Player(dbus.service.Object):
    @dbus.service.signal('org.freedesktop.DBus.Properties', signature='sa{sv}as')
    def PropertiesChanged(self, interface, changed, invalidated): pass
    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='s', out_signature='a{sv}')
    def GetAll(self, interface): return {'Status': status}
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
        snapshot = {'/player': {iface: {'Status': status}}}
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
        elif command == 'invalidate': player.PropertiesChanged(iface, {}, ['Status'])
        elif command == 'restart':
            present = False
            bus.release_name('org.bluez')
            GLib.timeout_add(50, lambda: (bus.request_name('org.bluez'), False)[1])
root = Root(bus, '/')
player = Player(bus, '/player')
open(os.environ['BLUEZ_TEST_READY'], 'w').close()
GLib.MainLoop().run()
