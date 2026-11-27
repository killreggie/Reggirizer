#pragma once

#include <juce_core/juce_core.h>
#include "BasedUIData.h"

// The bundled UI files and how a web-view request maps onto them.
//
// JUCE hands the resource provider a *path* such as "/fonts/LilitaOne.ttf" or "/"
// (some back ends may include the scheme and host). Every bundled file is found by
// its file name, so "/dvd/studio.jpg" and "/fonts/LilitaOne.ttf" both work.
namespace BasedUIFiles
{
    inline juce::String fileFor (juce::String url)
    {
        if (url.contains ("://"))                                   // juce://juce.backend/x -> /x
            url = url.fromFirstOccurrenceOf ("://", false, false).fromFirstOccurrenceOf ("/", true, false);
        url = url.upToFirstOccurrenceOf ("?", false, false).upToFirstOccurrenceOf ("#", false, false);
        const auto file = url.fromLastOccurrenceOf ("/", false, false);
        return file.isEmpty() ? juce::String ("index.html") : file;
    }

    // Returns the bundled bytes for a file name, or nullptr if it isn't bundled.
    inline const char* find (const juce::String& fileName, int& size)
    {
        for (int i = 0; i < BasedUIData::namedResourceListSize; ++i)
            if (fileName == BasedUIData::originalFilenames[i])
                return BasedUIData::getNamedResource (BasedUIData::namedResourceList[i], size);
        size = 0;
        return nullptr;
    }

    inline const char* mimeFor (const juce::String& file)
    {
        const auto ext = file.fromLastOccurrenceOf (".", false, false).toLowerCase();
        if (ext == "html") return "text/html; charset=utf-8";
        if (ext == "js")   return "text/javascript; charset=utf-8";
        if (ext == "css")  return "text/css; charset=utf-8";
        if (ext == "png")  return "image/png";
        if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
        if (ext == "svg")  return "image/svg+xml";
        if (ext == "ttf")  return "font/ttf";
        if (ext == "json") return "application/json";
        return "application/octet-stream";
    }
}
