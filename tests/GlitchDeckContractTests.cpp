#include "GlitchDeckPlugin.hpp"
#include "UiValueText.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); std::abort(); } } while (false)
namespace
{
struct Host
{
    bool audio = false;
    unsigned callbacks = 0;
    clap_host_t api {};
    clap_host_params_t params {};
    clap_host_thread_check_t threads {};
    Host()
    {
        api = { CLAP_VERSION, this, "GlitchDeck contracts", "Null Exo", "", "1",
            [](const clap_host_t* h, const char* id) -> const void* {
                auto& self = *static_cast<Host*>(h->host_data);
                if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) return &self.params;
                if (std::strcmp(id, CLAP_EXT_THREAD_CHECK) == 0) return &self.threads;
                return nullptr;
            }, [](const clap_host_t*) {}, [](const clap_host_t*) {},
            [](const clap_host_t* h) { ++static_cast<Host*>(h->host_data)->callbacks; } };
        params.rescan = [](const clap_host_t* h, clap_param_rescan_flags) { CHECK(!static_cast<Host*>(h->host_data)->audio); };
        params.clear = [](const clap_host_t*, clap_id, clap_param_clear_flags) {};
        params.request_flush = [](const clap_host_t* h) { CHECK(!static_cast<Host*>(h->host_data)->audio); };
        threads.is_main_thread = [](const clap_host_t* h) { return !static_cast<Host*>(h->host_data)->audio; };
        threads.is_audio_thread = [](const clap_host_t* h) { return static_cast<Host*>(h->host_data)->audio; };
    }
};
struct Input
{
    std::vector<const clap_event_header_t*> events;
    clap_input_events_t api { this,
        [](const clap_input_events_t* in) { return static_cast<std::uint32_t>(static_cast<Input*>(in->ctx)->events.size()); },
        [](const clap_input_events_t* in, std::uint32_t n) { return static_cast<Input*>(in->ctx)->events[n]; } };
};
struct Output
{
    struct Record { std::uint16_t type; clap_id id; double value; };
    std::array<Record, 2048> events {};
    std::size_t count = 0, capacity = events.size();
    clap_output_events_t api { this, [](const clap_output_events_t* out, const clap_event_header_t* event) {
        auto& self = *static_cast<Output*>(out->ctx);
        if (self.count >= self.capacity) return false;
        auto& record = self.events[self.count++];
        record.type = event->type;
        if (event->type == CLAP_EVENT_PARAM_VALUE)
        {
            const auto& value = *reinterpret_cast<const clap_event_param_value_t*>(event);
            record.id = value.param_id; record.value = value.value;
        }
        else record.id = reinterpret_cast<const clap_event_param_gesture_t*>(event)->param_id;
        return true;
    }};
};
clap_event_midi_t cc(int number, int value, int channel = 16, unsigned time = 0)
{
    clap_event_midi_t event {};
    event.header = { sizeof(event), time, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI, 0 };
    event.data[0] = static_cast<std::uint8_t>(0xb0 + channel - 1);
    event.data[1] = static_cast<std::uint8_t>(number); event.data[2] = static_cast<std::uint8_t>(value);
    return event;
}
clap_event_note_t note(int key, int channel, int id, bool down, unsigned time = 0)
{
    clap_event_note_t event {};
    event.header = { sizeof(event), time, CLAP_CORE_EVENT_SPACE_ID,
        static_cast<std::uint16_t>(down ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF), 0 };
    event.port_index = 0; event.key = static_cast<std::int16_t>(key);
    event.channel = static_cast<std::int16_t>(channel - 1); event.note_id = id;
    event.velocity = down ? 0.7 : 0.0;
    return event;
}
clap_event_param_value_t parameter(clap_id id, double value, unsigned time = 0)
{
    clap_event_param_value_t event {};
    event.header = { sizeof(event), time, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0 };
    event.param_id = id; event.value = value;
    event.note_id = event.port_index = event.channel = event.key = -1;
    return event;
}
struct Fixture
{
    Host host;
    GlitchDeckPlugin* plugin = new GlitchDeckPlugin(&host.api);
    const clap_plugin_t* api = plugin->clapPlugin();
    std::array<float, 2048> left {}, right {}, outLeft {}, outRight {};
    float* inputs[2] { left.data(), right.data() };
    float* outputs[2] { outLeft.data(), outRight.data() };
    clap_audio_buffer_t in {}, out {};
    clap_event_transport_t transport {};
    bool useTransport = false;
    double beat = 0.0;
    Fixture()
    {
        CHECK(api->init(api));
        in.data32 = inputs; in.channel_count = 2;
        out.data32 = outputs; out.channel_count = 2;
        left.fill(0.4f); right.fill(0.7f);
        transport.header = { sizeof(transport), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_TRANSPORT, 0 };
        transport.flags = CLAP_TRANSPORT_IS_PLAYING | CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE | CLAP_TRANSPORT_HAS_TIME_SIGNATURE;
        transport.tempo = 120; transport.tsig_num = 4; transport.tsig_denom = 4;
        for (int slot = 0; slot < 8; ++slot)
        {
            const auto& id = plugin->slotIds(slot);
            set(id.attack, 0.1); set(id.release, 0.1);
        }
        CHECK(api->activate(api, 1000, 1, 2048));
        host.audio = true; CHECK(api->start_processing(api)); host.audio = false;
    }
    ~Fixture()
    {
        host.audio = true; api->stop_processing(api); host.audio = false;
        api->deactivate(api); api->destroy(api);
    }
    void set(clap_id id, double value) { CHECK(plugin->parameters().setBaseValue(id, value)); }
    void block(unsigned frames = 1, std::initializer_list<const clap_event_header_t*> events = {}, Output* output = nullptr)
    {
        Input input; input.events = events;
        Output unused;
        clap_process_t block {};
        block.frames_count = frames; block.in_events = &input.api;
        block.out_events = output ? &output->api : &unused.api;
        block.audio_inputs = &in; block.audio_outputs = &out;
        block.audio_inputs_count = block.audio_outputs_count = 1;
        if (useTransport)
        {
            transport.song_pos_beats = static_cast<clap_beattime>(std::llround(beat * CLAP_BEATTIME_FACTOR));
            block.transport = &transport;
        }
        out.constant_mask = 3;
        host.audio = true;
        CHECK(api->process(api, &block) == CLAP_PROCESS_CONTINUE);
        host.audio = false;
        CHECK(out.constant_mask == 0);
        if (useTransport && (transport.flags & CLAP_TRANSPORT_IS_PLAYING) != 0)
            beat += (frames * transport.tempo + 0.5 * frames * (frames - 1.0) * transport.tempo_inc) / 60000.0;
        transport.tempo += frames * transport.tempo_inc;
    }
    void service() { api->on_main_thread(api); }
    bool active(int slot = 0) const { return plugin->isSlotActive(slot); }
};
void midiAndSources()
{
    Fixture f;
    auto down = cc(20, 127), up = cc(20, 0), wrong = cc(20, 127, 1);
    f.block(1, { &wrong.header }); CHECK(!f.active());
    f.block(1, { &down.header }); CHECK(f.active());
    CHECK(f.plugin->midiActivityText().find("CC20 CH16 127") != std::string::npos);
    f.plugin->setTriggerFromUi(0, true); f.block();
    f.block(1, { &up.header }); CHECK(f.active()); // Mouse still owns the hold.
    f.plugin->setTriggerFromUi(0, true, GlitchDeckPlugin::UiSource::keyboard);
    f.plugin->setTriggerFromUi(0, false); f.block(); CHECK(f.active());
    f.plugin->setTriggerFromUi(0, false, GlitchDeckPlugin::UiSource::keyboard); f.block(); CHECK(!f.active());
    f.set(f.plugin->slotIds(0).midiChannel, 0); f.block();
    auto ch1 = cc(20, 127, 1), ch1off = cc(20, 0, 1);
    f.block(1, { &down.header, &ch1.header }); CHECK(f.active());
    f.block(1, { &up.header }); CHECK(f.active());
    f.block(1, { &ch1off.header }); CHECK(!f.active());
    down.port_index = 1;
#if defined(NDEBUG)
    f.block(1, { &down.header }); CHECK(!f.active());
#endif
}
void latchAndEmergencyRelease()
{
    Fixture f;
    f.set(f.plugin->slotIds(0).latch, 1); f.block();
    auto down = cc(20, 127), up = cc(20, 0), stop = cc(123, 0);
    f.block(1, { &down.header }); CHECK(f.active());
    f.block(1, { &down.header }); CHECK(f.active()); // Duplicate 127 is not another press.
    f.block(1, { &up.header }); CHECK(f.active());
    f.block(1, { &down.header }); CHECK(!f.active());
    f.block(1, { &up.header, &down.header }); CHECK(f.active());
    f.block(1, { &stop.header }); CHECK(!f.active());
    f.block(1, { &down.header }); CHECK(f.active());
    f.plugin->panicFromUi(); f.block(); CHECK(!f.active());
    f.plugin->setTriggerFromUi(0, true); f.block(); CHECK(f.active());
    f.plugin->releaseUiTriggers(); f.block(); CHECK(f.active()); // Closing does not disable a deliberate latch.
    f.plugin->panicFromUi(); f.block(); CHECK(!f.active());
}
void nativeAndRawNotes()
{
    Fixture f;
    const auto& id = f.plugin->slotIds(0);
    f.set(id.midiType, 0); f.set(id.midiNumber, 60); f.block();
    auto a = note(60,16,12,true), b = note(60,16,13,true), off = note(60,16,12,false);
    a.velocity = 0.0;
    f.block(1, { &a.header, &b.header }); CHECK(f.active());
    f.block(1, { &off.header }); CHECK(f.active());
    off.note_id = -1; f.block(1, { &off.header }); CHECK(!f.active());
    f.block(1, { &a.header, &b.header });
    off.header.type = CLAP_EVENT_NOTE_CHOKE; off.note_id = 12;
    f.block(1, { &off.header }); CHECK(f.active());
    off.note_id = -1; off.key = -1; off.channel = -1; off.port_index = -1;
    f.block(1, { &off.header }); CHECK(!f.active());
    auto raw = cc(60, 127); raw.data[0] = 0x9f;
    f.block(1, { &raw.header }); CHECK(f.active());
    raw.data[2] = 0; f.block(1, { &raw.header }); CHECK(!f.active());
    f.set(id.latch, 1); f.block();
    f.block(1, { &a.header });
    auto release = note(60,16,12,false); f.block(1, { &release.header }); CHECK(f.active());
    f.block(1, { &off.header }); CHECK(!f.active());
}
void sampleAccurateAndQuantized()
{
    Fixture f;
    const auto& id = f.plugin->slotIds(7);
    auto down = cc(27, 127, 16, 3), up = cc(27, 0, 16, 7);
    f.block(10, { &down.header, &up.header });
    for (int n=0; n<10; ++n) CHECK(std::abs(f.outLeft[n] - (n>=3 && n<7 ? 0.0f : 0.4f)) < 1e-6f);
    f.set(id.quantize, 2); f.useTransport = true; f.beat = 0.2;
    down.header.time = 0; up.header.time = 0;
    f.block(25, { &down.header }); CHECK(!f.active(7)); CHECK(f.plugin->isSlotPending(7));
    f.block(); CHECK(f.active(7)); CHECK(!f.plugin->isSlotPending(7));
    f.block(1, { &up.header }); CHECK(!f.active(7));
    f.beat = 0.2; f.block(1, { &down.header }); CHECK(f.plugin->isSlotPending(7));
    f.block(1, { &up.header }); CHECK(!f.plugin->isSlotPending(7));
    f.block(40); CHECK(!f.active(7));
    f.beat = 0.2; f.block(1, { &down.header });
    f.beat = 10.0; f.block(40); CHECK(!f.active(7)); CHECK(!f.plugin->isSlotPending(7));
    f.block(1, { &up.header }); f.beat = 0.2; f.block(1, { &down.header });
    f.transport.flags &= ~CLAP_TRANSPORT_IS_PLAYING; f.block(40); CHECK(!f.active(7));
    f.transport.flags |= CLAP_TRANSPORT_IS_PLAYING;
    f.block(1, { &up.header }); f.beat = 0.2; f.block(1, { &down.header });
    f.useTransport = false; f.block(40); CHECK(!f.active(7));
}
void automationAndConfigAtZero()
{
    Fixture f;
    for (int n = 0; n < 100; ++n) f.left[n] = f.right[n] = static_cast<float>(n);
    f.block(100);
    const auto& id = f.plugin->slotIds(0);
    auto length = parameter(id.length, 4), trigger = parameter(id.trigger, 1);
    f.block(1, { &length.header, &trigger.header });
    CHECK(f.active()); CHECK(std::abs(f.outLeft[0] - 94.0f) < 1e-4f);
    auto off = parameter(id.trigger, 0); f.block(1, { &off.header }); CHECK(!f.active());
#if defined(NDEBUG)
    trigger.note_id = 8; f.block(1, { &trigger.header }); CHECK(!f.active());
    trigger.note_id = -1; trigger.value = std::numeric_limits<double>::quiet_NaN();
    f.block(1, { &trigger.header }); CHECK(!f.active());
#endif
    auto number = parameter(id.midiNumber, 33); auto down = cc(33, 127);
    f.block(1, { &number.header, &down.header }); CHECK(f.active());
    f.set(id.midiNumber, 34); f.block(); CHECK(!f.active()); // Rebinding cannot strand an old note.
}
void learnWithoutEditorAndCancellation()
{
    Fixture f;
    f.plugin->toggleMidiLearn(2);
    auto message = cc(49,127,11);
    f.block(1, { &message.header }); CHECK(f.host.callbacks > 0); CHECK(!f.active(2));
    f.service();
    const auto& id = f.plugin->slotIds(2);
    CHECK(f.plugin->parameterInt(id.midiNumber) == 49 && f.plugin->parameterInt(id.midiChannel) == 11);
    f.plugin->toggleMidiLearn(2);
    auto old = cc(50,127,2); f.block(1, { &old.header });
    f.plugin->toggleMidiLearn(3); f.service();
    CHECK(f.plugin->parameterInt(id.midiNumber) == 49); CHECK(f.plugin->isMidiLearning(3));
    auto mode = cc(123,0); f.block(1, { &mode.header }); CHECK(f.plugin->isMidiLearning(3));
    auto press = note(62,5,1,true); f.block(1, { &press.header }); f.service();
    CHECK(f.plugin->parameterInt(f.plugin->slotIds(3).midiType) == 0);
    CHECK(f.plugin->parameterInt(f.plugin->slotIds(3).midiNumber) == 62);
}
void overflowAndGestureRetry()
{
    Fixture f;
    for (int n=0;n<200;++n) { f.plugin->setTriggerFromUi(0,true); f.plugin->setTriggerFromUi(0,false); }
    f.block(); CHECK(!f.active()); f.block(); CHECK(!f.active());
    f.plugin->setTriggerFromUi(0,true); f.block(); CHECK(f.active());
    f.plugin->releaseUiTriggers(); f.block(); CHECK(!f.active());
    // Drain previous trigger edits before examining the separate Mix gesture.
    for (int n=0;n<5;++n) { f.block(); f.service(); }
    f.plugin->beginUiEdit(glitchdeck::ids::mix);
    for (int n=0;n<1000;++n) f.plugin->setUiValue(glitchdeck::ids::mix, (n%100) / 100.0);
    f.plugin->setUiValue(glitchdeck::ids::mix, 0.37);
    f.plugin->endUiEdit(glitchdeck::ids::mix);
    int begins=0, ends=0; double last=-1;
    for (int n=0;n<300;++n)
    {
        Output out; out.capacity=1; f.block(1, {}, &out); f.service();
        for (std::size_t i=0;i<out.count;++i)
        {
            const auto& e=out.events[i]; if (e.id != glitchdeck::ids::mix) continue;
            begins += e.type == CLAP_EVENT_PARAM_GESTURE_BEGIN;
            ends += e.type == CLAP_EVENT_PARAM_GESTURE_END;
            if (e.type == CLAP_EVENT_PARAM_VALUE) last=e.value;
        }
    }
    CHECK(begins==1 && ends==1); CHECK(std::abs(last-0.37)<1e-9);
    CHECK(std::abs(f.plugin->parameterValue(glitchdeck::ids::mix)-0.37)<1e-9);
}

void stateRollbackAndDeferredReset()
{
    Fixture f;
    CHECK(f.plugin->parameters().count() == 105);
    struct Stream
    {
        std::vector<std::byte> bytes;
        std::size_t cursor = 0;
        clap_ostream_t out { this, [](const clap_ostream_t* stream, const void* data, std::uint64_t size) -> std::int64_t {
            auto& self = *static_cast<Stream*>(stream->ctx);
            const auto* first = static_cast<const std::byte*>(data);
            self.bytes.insert(self.bytes.end(), first, first + size);
            return static_cast<std::int64_t>(size);
        }};
        clap_istream_t in { this, [](const clap_istream_t* stream, void* data, std::uint64_t size) -> std::int64_t {
            auto& self = *static_cast<Stream*>(stream->ctx);
            const auto count = std::min<std::size_t>(static_cast<std::size_t>(size), self.bytes.size() - self.cursor);
            if (count) std::memcpy(data, self.bytes.data() + self.cursor, count);
            self.cursor += count; return static_cast<std::int64_t>(count);
        }};
    } good, invalid;
    const auto* state = static_cast<const clap_plugin_state_t*>(f.api->get_extension(f.api, CLAP_EXT_STATE));
    CHECK(state != nullptr && state->save(f.api, &good.out));
    const std::array payload { std::byte{0x42} };
    CHECK(nullclap::state::save(f.plugin->parameters(), payload, &invalid.out));
    f.set(glitchdeck::ids::mix, 0.3);
    CHECK(!state->load(f.api, &invalid.in));
    CHECK(f.plugin->parameterValue(glitchdeck::ids::mix) == 0.3);
    auto down = cc(20,127); f.block(1,{&down.header}); CHECK(f.active());
    CHECK(state->load(f.api, &good.in));
    CHECK(f.plugin->parameterValue(glitchdeck::ids::mix) == 1.0);
    CHECK(f.active()); // Main-thread state callback has not touched the audio-owned engine.
    f.block(); CHECK(!f.active());
}

void percentageTextAndLatchModeChange()
{
    using glitchdeck::ui::parsePercent;
    CHECK(parsePercent("50%", 0.82) == 0.5);
    CHECK(parsePercent("  +12.5 % ", 0.82) == 0.125);
    CHECK(parsePercent("50", 0.82) == 0.5);
    CHECK(parsePercent("100", 0.82) == 1.0);
    CHECK(parsePercent("150%", 0.82) == 1.0);
    CHECK(parsePercent("-50%", 0.82) == 0.0);
    for (const auto* text : { "", "%", "junk", "nan", "inf", "50garbage", "1e999", "50%%" })
        CHECK(parsePercent(text, 0.82) == 0.82);
    Fixture f;
    const auto& id = f.plugin->slotIds(0);
    f.set(id.latch, 1); f.block();
    auto down=cc(20,127), up=cc(20,0);
    f.block(1,{&down.header}); f.block(1,{&up.header}); CHECK(f.active());
    f.set(id.latch, 0); f.block(); CHECK(!f.active());
}

void tempoRampsAndPendingLatch()
{
    Fixture f;
    const auto& id=f.plugin->slotIds(7);
    f.set(id.quantize,2); f.useTransport=true; f.beat=0.2;
    f.transport.tempo=120; f.transport.tempo_inc=1.0;
    auto down=cc(27,127), up=cc(27,0);
    // Solve 120*n + n*(n-1)/2 >= 3000: onset at sample 23, not 22.
    f.block(23,{&down.header}); CHECK(!f.active(7)); CHECK(f.plugin->isSlotPending(7));
    f.block(); CHECK(f.active(7)); f.block(1,{&up.header});
    f.transport.tempo=120; f.transport.tempo_inc=0;
    f.beat=0.2; f.block(10,{&down.header}); CHECK(f.plugin->isSlotPending(7));
    f.transport.tempo=60; // Continuous beat position but half-speed remaining grid.
    f.block(30); CHECK(!f.active(7)); f.block(); CHECK(f.active(7));
    f.block(1,{&up.header}); f.set(id.latch,1); f.block();
    f.beat=0.2; f.block(1,{&down.header}); CHECK(f.plugin->isSlotPending(7));
    f.block(1,{&up.header}); f.block(1,{&down.header});
    CHECK(!f.plugin->isSlotPending(7)); f.block(100); CHECK(!f.active(7));
}

}
int main()
{
    midiAndSources(); latchAndEmergencyRelease(); nativeAndRawNotes(); sampleAccurateAndQuantized();
    automationAndConfigAtZero(); learnWithoutEditorAndCancellation(); overflowAndGestureRetry();
    percentageTextAndLatchModeChange(); tempoRampsAndPendingLatch(); stateRollbackAndDeferredReset();
    std::puts("GlitchDeck MIDI, timing, source ownership, learn and backpressure contracts passed");
}
