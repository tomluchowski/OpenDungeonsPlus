"""Cosmetic events: format, round trip, negotiation, old clients and servers; never launches a game."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])


def read(path):
    return (root / path).read_text(encoding='utf-8')


notification_h = read('source/network/ServerNotification.h')
body = notification_h[notification_h.index('enum class ServerNotificationType'):]
body = body[:body.index('};')]
names = [m.group(1) for m in re.finditer(r'^\s*(\w+),?\s*(?://.*)?$', body, re.MULTILINE)
         if m.group(1) not in ('enum', 'class')]
# The new kind is appended last, exit stays in front of it: no existing number moves
assert names[-1] == 'cosmeticEvent' and names[-2] == 'exit', names[-3:]
assert 'case ServerNotificationType::cosmeticEvent:' in read('source/network/ServerNotification.cpp')

server = read('source/network/ODServer.cpp')
client = read('source/network/ODClient.cpp')
socket_h = read('source/network/ODSocketClient.h')
# The offer is the flag of pickNick behind the server mode, the agreement is read only when the packet has it
assert 'pickNick << mServerMode << true;' in server
assert 'if(!packetReceived.endOfPacket())\n                OD_ASSERT_TRUE(packetReceived >> cosmeticEvents);' in server
assert 'clientSocket->setSupportsCosmeticEvents(cosmeticEvents);' in server
assert server.count('<< clientSocket->supportsCosmeticEvents();') == 1
assert server.count('<< client->supportsCosmeticEvents();') == 1
assert 'if(!packetReceived.endOfPacket())\n                OD_ASSERT_TRUE(packetReceived >> cosmeticEvents);' in client
assert 'setSupportsCosmeticEvents(cosmeticEvents);' in client
assert 'mSupportsCosmeticEvents(false)' in socket_h
assert 'mSupportsCosmeticEvents = false;' in read('source/network/ODSocketClient.cpp')
# Nothing is queued for a player who did not agree
send = server[server.index('void ODServer::sendCosmeticEvent('):]
send = send[:send.index('\n}\n')]
assert 'supportsCosmeticEvents(player)' in send and 'getIsHuman()' in send
# The client ignores events without the agreement and kinds it does not know
handler = client[client.index('case ServerNotificationType::cosmeticEvent:'):]
handler = handler[:handler.index('case ServerNotificationType::entitiesRefresh:')]
assert '!supportsCosmeticEvents() || !event.isKnownType()' in handler

# Cosmetic only: the emitters do not touch the activity snapshot or the refresh flag
creature = read('source/entities/Creature.cpp')
for signature in ('void Creature::fireCosmeticEvent(const CosmeticEvent& event', 'void Creature::fireArrivalEvent()'):
    function = creature[creature.index(signature):]
    function = function[:function.index('\n}\n')]
    assert 'mActivity' not in function and 'mNeedFireRefresh' not in function and 'fireCreatureRefreshIfNeeded' not in function, signature

probe = r'''
#include "network/CosmeticEvent.h"
#include <iostream>
#include <string>
#include <vector>

static int checks = 0, failures = 0;
static void check(bool ok, const char* why) { ++checks; if(!ok) { ++failures; std::cerr << "FAIL " << why << '\n'; } }

// What a client from before the cosmetic events does with a message: a switch without the kind ends in
// the default branch, logs and goes on with the next packet.
enum class OldNotification : int32_t { chat = 0, turnStarted = 1, timeLimit = 2 };
static bool oldClientProcess(int32_t cmd, ODPacket& packet, int& handled)
{
    switch(static_cast<OldNotification>(cmd))
    {
        case OldNotification::timeLimit:
        {
            int32_t seconds = 0;
            packet >> seconds;
            ++handled;
            break;
        }
        default:
            break;
    }
    return true;
}

int main()
{
    // Round trip of every kind, with the sentinel behind it to prove nothing is left over or missing
    for(int32_t type = 0; type <= 18; ++type)
    {
        // 9 to 16 are kept free for the kinds of other features (they are no kinds here)
        if((type >= 9) && (type <= 16))
            continue;
        CosmeticEvent event;
        event.mType = type;
        event.mSubject = "Orc_3";
        event.mObject = "Kobold_9";
        event.mText = "ArrowProjectile";
        event.mValue = -7 + type;
        event.mValue2 = 1000 - type;
        event.mPosition = Ogre::Vector3(1.5f, -2.25f, 3.0f);
        ODPacket packet;
        packet << event << uint32_t(0xCAFE);
        CosmeticEvent back;
        packet >> back;
        uint32_t sentinel = 0;
        packet >> sentinel;
        check(back.mType == type && back.mSubject == event.mSubject && back.mObject == event.mObject &&
              back.mText == event.mText && back.mValue == event.mValue && back.mValue2 == event.mValue2 &&
              back.mPosition == event.mPosition, "round trip of every field");
        check(sentinel == 0xCAFE && packet.endOfPacket(), "packet stays aligned");
        check(back.isKnownType() && back.typeString() != "unknown", "known kinds have names");
    }

    // A kind of a newer build: read completely, reported unknown, nothing lost behind it
    CosmeticEvent future;
    future.mType = 4711;
    future.mSubject = "x";
    ODPacket futurePacket;
    futurePacket << future << std::string("next");
    CosmeticEvent readFuture;
    futurePacket >> readFuture;
    std::string next;
    futurePacket >> next;
    check(!readFuture.isKnownType() && readFuture.typeString() == "unknown" && next == "next", "unknown kind is skipped cleanly");
    CosmeticEvent empty;
    check(!empty.isKnownType(), "default event is no kind");

    // A cut packet is an error, not a crash
    ODPacket whole;
    CosmeticEvent sample(CosmeticEventType::treasuryFull);
    sample.mObject = "Treasury_1";
    whole << sample;
    ODPacket cut;
    cut << int32_t(3) << std::string("only half");
    CosmeticEvent half;
    cut >> half;
    check(!static_cast<bool>(cut), "truncated event reports an error");
    check(CosmeticEvent(CosmeticEventType::scared).is(CosmeticEventType::scared), "is()");

    // An old client gets nothing (it never agreed), but even if one arrived it would be skipped, and the next
    // message is still read correctly
    ODPacket stream1, stream2;
    stream1 << int32_t(9999) << sample;
    stream2 << int32_t(OldNotification::timeLimit) << int32_t(120);
    int handled = 0;
    int32_t cmd = 0;
    stream1 >> cmd;
    check(oldClientProcess(cmd, stream1, handled) && handled == 0, "old client skips the unknown message");
    stream2 >> cmd;
    check(oldClientProcess(cmd, stream2, handled) && handled == 1, "old client goes on with the next message");

    // Negotiation: an old server ends the offer after the server mode, an old client ends its answer after the nick
    ODPacket oldOffer;
    bool offered = false;
    if(!oldOffer.endOfPacket())
        oldOffer >> offered;
    check(!offered, "a client does not agree to what an old server does not offer");
    ODPacket oldAnswer;
    oldAnswer << std::string("nick");
    std::string nick;
    oldAnswer >> nick;
    bool cosmetic = false;
    if(!oldAnswer.endOfPacket())
        oldAnswer >> cosmetic;
    check(!cosmetic, "an old client never gets cosmetic events");
    ODPacket newAnswer;
    newAnswer << std::string("nick") << true;
    newAnswer >> nick;
    if(!newAnswer.endOfPacket())
        newAnswer >> cosmetic;
    check(cosmetic, "a new client agrees");

    std::cout << "CHECKS=" << checks << " FAILURES=" << failures << '\n';
    return failures ? 1 : 0;
}
'''

with tempfile.TemporaryDirectory(prefix='odp-cosmetic-') as directory:
    work = Path(directory)
    (work / 'probe.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', f'/I{root / "source"}',
                    f'/I{prefix / "include"}', f'/I{prefix / "include/OGRE"}', 'probe.cpp',
                    str(root / 'source/network/CosmeticEvent.cpp'), str(root / 'source/network/ODPacket.cpp'),
                    '/Feprobe.exe', '/link', f'/LIBPATH:{prefix / "lib"}', 'sfml-network.lib', 'sfml-system.lib',
                    'OgreMain.lib'], cwd=work, check=True)
    subprocess.run([str(work / 'probe.exe')], cwd=work, check=True)
