#include <mosqueeze/AlgorithmSelector.hpp>
#include <mosqueeze/FileTypeDetector.hpp>

#include <cassert>
#include <iostream>

int main() {
    mosqueeze::FileTypeDetector detector;
    mosqueeze::AlgorithmSelector selector;

    const auto aviClass = detector.detectFromExtension(".avi");
    assert(aviClass.type == mosqueeze::FileType::Video_AVI);
    assert(aviClass.canRecompress);

    const auto aviSelection = selector.select(aviClass);
    assert(aviSelection.algorithm == "zpaq");
    assert(aviSelection.level == 5);
    assert(!aviSelection.shouldSkip);
    std::cout << "[PASS] AVI -> ZPAQ level 5\n";

    const auto mkvClass = detector.detectFromExtension(".mkv");
    assert(mkvClass.type == mosqueeze::FileType::Video_MKV);

    const auto mkvSelection = selector.select(mkvClass);
    assert(mkvSelection.algorithm == "zpaq");
    assert(mkvSelection.level == 5);
    std::cout << "[PASS] MKV -> ZPAQ level 5\n";

    const auto mp4Class = detector.detectFromExtension(".mp4");
    assert(mp4Class.type == mosqueeze::FileType::Video_MP4);

    const auto mp4Selection = selector.select(mp4Class);
    assert(mp4Selection.shouldSkip);
    std::cout << "[PASS] MP4 -> skip\n";

    const auto webmClass = detector.detectFromExtension(".webm");
    assert(webmClass.type == mosqueeze::FileType::Video_WebM);

    const auto webmSelection = selector.select(webmClass);
    assert(webmSelection.shouldSkip);
    std::cout << "[PASS] WebM -> skip\n";

    return 0;
}
