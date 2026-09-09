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
invalidation_race = False
fail_snapshot = False
transfer_mode = 'active'
session_present = True
create_session_calls = 0
registrations = {}

def current_track():
    track = {'Title': 'Blue Train'}
    if transfer_mode != 'owned_native_image' or session_present:
        track['ImgHandle'] = 'image'
    return dbus.Dictionary(track, signature='sv')

class Player(dbus.service.Object):
    @dbus.service.signal('org.freedesktop.DBus.Properties', signature='sa{sv}as')
    def PropertiesChanged(self, interface, changed, invalidated): pass
    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='s', out_signature='a{sv}')
    def GetAll(self, interface):
        global invalidation_race
        if invalidation_race:
            invalidation_race = False
            self.PropertiesChanged(iface, {'Position': dbus.UInt32(42)}, [])
        return {'Status': status, 'Device': dbus.ObjectPath('/device'),
                'ObexPort': dbus.UInt16(4097),
                'Track': current_track()}

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
        global status, fail_snapshot
        if fail_snapshot:
            fail_snapshot = False
            raise dbus.exceptions.DBusException('Snapshot denied',
                name='org.freedesktop.DBus.Error.AccessDenied')
        if not present: return {}
        snapshot = {'/player': {iface: {
            'Status': status,
            'Device': dbus.ObjectPath('/device'),
            'ObexPort': dbus.UInt16(4097),
            'Track': current_track(),
        }}}
        if session_present:
            snapshot['/session'] = {'org.bluez.obex.Session1': {
                'Destination': '00:11:22:33:44:55', 'PSM': dbus.UInt16(4097)}}

        status = 'playing'
        player.PropertiesChanged(iface, {'Status': status}, [])
        return snapshot
    @dbus.service.signal('org.freedesktop.DBus.ObjectManager', signature='oas')
    def InterfacesRemoved(self, path, interfaces): pass
    @dbus.service.method('review.Test', in_signature='s')
    def Step(self, command):
        global present, status, invalidation_race, fail_snapshot, transfer_mode
        global session_present, create_session_calls
        if command == 'reset':
            present = True
            status = 'paused'
            transfer_mode = 'active'
            session_present = True
            create_session_calls = 0
        elif command == 'owned_native_image':
            transfer_mode = 'owned_native_image'
            session_present = False
        elif command == 'restart_obex':
            session_present = False
            bus.release_name('org.bluez.obex')
            GLib.timeout_add(50, lambda: (bus.request_name('org.bluez.obex'), False)[1])
        elif command == 'remove_obex_session':
            session_present = False
            self.InterfacesRemoved('/session', ['org.bluez.obex.Session1'])
        elif command == 'fast_complete_without_size':
            transfer_mode = 'fast_complete_without_size'
        elif command == 'image_error':
            transfer_mode = 'image_error'
        elif command == 'fail_snapshot': fail_snapshot = True
        elif command == 'invalidate_race':
            status = 'stopped'
            invalidation_race = True
            player.PropertiesChanged(iface, {}, ['Status'])
        elif command == 'assert_cancelled':
            if transfer.cancelled == 0: raise RuntimeError('Transfer was not cancelled')
        elif command == 'assert_session_created':
            if create_session_calls != 1: raise RuntimeError('Session was not created exactly once')
        elif command == 'assert_session_recreated':
            if create_session_calls != 2: raise RuntimeError('Session was not recreated exactly once')
        elif command == 'assert_removed_session_recreated':
            if create_session_calls != 3: raise RuntimeError('Removed session was not recreated exactly once')
        elif command == 'invalidate': player.PropertiesChanged(iface, {}, ['Status'])
        elif command in ('restart', 'restart_present'):
            present = command == 'restart_present'
            status = 'paused'
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

class ObexClient(dbus.service.Object):
    @dbus.service.method('org.bluez.obex.Client1', in_signature='sa{sv}', out_signature='o')
    def CreateSession(self, destination, args):
        global session_present, create_session_calls
        if (destination != '00:11:22:33:44:55' or
                args.get('Target') != 'bip-avrcp' or int(args.get('PSM', 0)) != 4097):
            raise dbus.exceptions.DBusException(
                'Invalid session arguments', name='org.bluez.obex.Error.InvalidArguments')
        session_present = True
        create_session_calls += 1
        player.PropertiesChanged(iface, {'Track': current_track()}, [])
        return '/session'

    @dbus.service.method('org.bluez.obex.Client1', in_signature='o')
    def RemoveSession(self, path):
        global session_present
        session_present = False

class Image(dbus.service.Object):
    @dbus.service.method('org.bluez.obex.Image1', in_signature='ssa{sv}', out_signature='oa{sv}')
    def Get(self, target, handle, description):
        if transfer_mode == 'image_error':
            raise dbus.exceptions.DBusException(
                'Remote player rejected the image request',
                name='org.bluez.obex.Error.NotSupported')
        if transfer_mode != 'owned_native_image' or description:
            raise dbus.exceptions.DBusException(
                'Remote player rejected the image request',
                name='org.bluez.obex.Error.NotSupported')
        with open(target, 'wb') as file: file.write(b'cover-art')
        transfer.cancelled = 0
        return '/transfer', {}

    @dbus.service.method('org.bluez.obex.Image1', in_signature='ss', out_signature='oa{sv}')
    def GetThumbnail(self, target, handle):
        if transfer_mode == 'image_error':
            raise dbus.exceptions.DBusException(
                'Remote player rejected the image request',
                name='org.bluez.obex.Error.NotSupported')
        content = b'cover-art' if transfer_mode == 'fast_complete_without_size' else b'partial'
        with open(target, 'wb') as file: file.write(content)
        transfer.cancelled = 0
        properties = {} if transfer_mode == 'fast_complete_without_size' else {'Size': dbus.UInt64(100)}
        return '/transfer', properties

    @dbus.service.method('org.bluez.obex.Image1', in_signature='s', out_signature='aa{sv}')
    def Properties(self, handle):
        raise dbus.exceptions.DBusException(
            'Remote player rejected the image request',
            name='org.bluez.obex.Error.NotSupported')

class Transfer(dbus.service.Object):
    cancelled = 0
    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='ss', out_signature='v')
    def Get(self, interface, prop):
        if transfer_mode in ('fast_complete_without_size', 'owned_native_image'):
            raise dbus.exceptions.DBusException(
                'Transfer already removed',
                name='org.freedesktop.DBus.Error.UnknownObject')
        return 'active'
    @dbus.service.method('org.bluez.obex.Transfer1')
    def Cancel(self): self.cancelled += 1

device = Device(bus, '/device')
obex_client = ObexClient(bus, '/org/bluez/obex')
image = Image(bus, '/session')
transfer = Transfer(bus, '/transfer')
root = Root(bus, '/')
player = Player(bus, '/player')
open(os.environ['BLUEZ_TEST_READY'], 'w').close()
GLib.MainLoop().run()
