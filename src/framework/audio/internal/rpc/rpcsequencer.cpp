//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2020 MuseScore BVBA and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================
#include "rpcsequencer.h"

#include "log.h"

using namespace mu::async;
using namespace mu::audio;
using namespace mu::audio::rpc;

RpcSequencer::RpcSequencer()
{
    m_target = Target(TargetName::Sequencer);
}

RpcSequencer::~RpcSequencer()
{
    rpcChannel()->unlisten(m_listenID);
}

void RpcSequencer::setup()
{
    //! NOTE It's not a very elegant solution - it's not good that we're recieving all messages here.
    //! It would be possible to do as it was done before - to subscribe for a given target and, accordingly, receive messages here only for this target.
    //! But this means that the channel assumes part of the responsibility of the controller,
    //! does more than is required - it makes a mapping of the messages and targets.
    //! We can explicitly add a controller, but I think this is overengineering.
    m_listenID = rpcChannel()->listen([this](const Msg& msg) {
        if (msg.target != m_target) {
            return;
        }

        using Call = std::function<void (const Args& args)>;
        static std::map<Method, Call> calls;
        auto bindMethod = [](std::map<Method, Call>& calls, const Method& method, const Call& call) {
            calls.insert({ method, call });
        };

        if (calls.empty()) {
            // Bind from real sequencer

            bindMethod(calls, "statusChanged", [this](const Args& args) {
                m_status = args.arg<Status>(0);
                m_statusChanged.send(m_status);
            });

            bindMethod(calls, "positionChanged", [this](const Args& args) {
                m_playbackPosition = args.arg<float>(0);
                m_positionChanged.notify();
            });

            bindMethod(calls, "midiTickPlayed", [this](const Args& args) {
                track_id trackID = args.arg<track_id>(0);
                midi::tick_t tick = args.arg<midi::tick_t>(1);
                m_midiTickPlayed[trackID].send(tick);
            });
        }

        auto it = calls.find(msg.method);
        if (it == calls.end()) {
            LOGE() << "not found method: " << msg.method;
            return;
        }

        it->second(msg.args);
    });
}

ISequencer::Status RpcSequencer::status() const
{
    return m_status;
}

Channel<ISequencer::Status> RpcSequencer::statusChanged() const
{
    return m_statusChanged;
}

void RpcSequencer::initMIDITrack(track_id id)
{
    rpcChannel()->send(Msg(m_target, "initMIDITrack", Args::make_arg1<track_id>(id)));
}

void RpcSequencer::initAudioTrack(track_id id)
{
    rpcChannel()->send(Msg(m_target, "initAudioTrack", Args::make_arg1<track_id>(id)));
}

void RpcSequencer::setMIDITrack(track_id id, const std::shared_ptr<midi::MidiStream>& stream)
{
    if (rpcChannel()->isSerialized()) {
        NOT_IMPLEMENTED;
    } else {
        rpcChannel()->send(Msg(m_target, "setMIDITrack", Args::make_arg2<track_id, std::shared_ptr<midi::MidiStream> >(id, stream)));
    }
}

void RpcSequencer::setAudioTrack(track_id id, const std::shared_ptr<audio::IAudioStream>& stream)
{
    if (rpcChannel()->isSerialized()) {
        NOT_IMPLEMENTED;
    } else {
        rpcChannel()->send(Msg(m_target, "setAudioTrack", Args::make_arg2<track_id, std::shared_ptr<audio::IAudioStream> >(id, stream)));
    }
}

void RpcSequencer::play()
{
    rpcChannel()->send(Msg(m_target, "play"));
}

void RpcSequencer::pause()
{
    rpcChannel()->send(Msg(m_target, "pause"));
}

void RpcSequencer::stop()
{
    rpcChannel()->send(Msg(m_target, "stop"));
}

void RpcSequencer::seek(uint64_t position)
{
    rpcChannel()->send(Msg(m_target, "seek", Args::make_arg1<uint64_t>(position)));
}

void RpcSequencer::rewind()
{
    rpcChannel()->send(Msg(m_target, "rewind"));
}

void RpcSequencer::setLoop(uint64_t fromMiliSeconds, uint64_t toMiliSeconds)
{
    rpcChannel()->send(Msg(m_target, "setLoop", Args::make_arg2<uint64_t, uint64_t>(fromMiliSeconds, toMiliSeconds)));
}

void RpcSequencer::unsetLoop()
{
    rpcChannel()->send(Msg(m_target, "unsetLoop"));
}

Channel<mu::midi::tick_t> RpcSequencer::midiTickPlayed(track_id id) const
{
    auto found = m_midiTickPlayed.find(id);
    if (found == m_midiTickPlayed.end()) {
        rpcChannel()->send(Msg(m_target, "bindMidiTickPlayed", Args::make_arg1<track_id>(id)));

        //! TODO Figure out if and when to remove not up to date channels
        m_midiTickPlayed.insert({ id, async::Channel<mu::midi::tick_t>() });
    }
    return m_midiTickPlayed[id];
}

Notification RpcSequencer::positionChanged() const
{
    return m_positionChanged;
}

float RpcSequencer::playbackPosition() const
{
    return m_playbackPosition;
}

ISequencer::midi_track_t RpcSequencer::instantlyPlayMidi(const midi::MidiData& data)
{
    if (rpcChannel()->isSerialized()) {
        NOT_IMPLEMENTED;
    } else {
        rpcChannel()->send(Msg(m_target, "instantlyPlayMidi", Args::make_arg1<midi::MidiData>(data)));
    }

    //! TODO It is necessary to remove the return of the pointer to the player object.
    //! This is thread-unsafe code, see https://github.com/musescore/MuseScore/pull/6848#discussion_r558445611
    //! If we need to control this playback, then we need to
    //! add the generation of the ID,
    //! transfer it to the real sequencer,
    //! return this ID from the method,
    //! and add methods for control by passing this ID to them.
    return nullptr;
}
