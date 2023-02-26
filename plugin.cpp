#include <spdlog/sinks/basic_file_sink.h>

namespace logger = SKSE::log;

void SetupLog() {
    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) {
        SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
        return;
    }
    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));
    spdlog::set_default_logger(std::move(loggerPtr));
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::info);
}

class EventHandler :
    public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    EventHandler() = default;
    ~EventHandler() = default;
    EventHandler(const EventHandler&) = delete;
    EventHandler(EventHandler&&) = delete;
    EventHandler& operator=(const EventHandler&) = delete;
    EventHandler& operator=(EventHandler&&) = delete;

public:
    static EventHandler& GetSingleton() {
        static EventHandler singleton;
        return singleton;
    }

    bool CheckActor(const RE::Actor *actor, const RE::PlayerCharacter *player) const {
        return  actor &&
                actor != player &&
                actor->Is3DLoaded() &&
                actor->GetCurrentLocation() == player->GetCurrentLocation() &&
                strcmp(actor->GetName(), "Boethiah Cultist") == 0;
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::MenuOpenCloseEvent* event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
    {
        if (!event || event->opening || event->menuName != "Loading Menu") {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto location = player->GetCurrentLocation();
        if (!location) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // check if we are in the arcanaeum
        if (location->GetFormID() != 0x1eb79) {
            return RE::BSEventNotifyControl::kContinue;
        }

        logger::info("{} is in the Arcanaeum. Looking for cultists...", player->GetName());
        auto forms = RE::TESForm::GetAllForms();
        forms.second.get().LockForRead();
        uint8_t count = 0;
        for (const auto &[key, val] : *(forms.first)) {
            RE::Actor *actor = val->As<RE::Actor>();
            if (CheckActor(actor, player)) {
                actor->SetDelete(true);
                count++;
            }
        }
        forms.second.get().UnlockForRead();

        if (count != 0) {
            auto msg = std::format("Deleted {} cultists", count);
            logger::info("{}", msg);
            RE::DebugNotification(msg.c_str());
        }

        return RE::BSEventNotifyControl::kContinue;
    }
};

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    SetupLog();

    auto *ui_source = RE::UI::GetSingleton();
    if (!ui_source) {
        return false;
    }

    auto &event_handler = EventHandler::GetSingleton();
    ui_source->AddEventSink<RE::MenuOpenCloseEvent>(&event_handler);

    return true;
}
