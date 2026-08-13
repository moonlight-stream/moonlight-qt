.pragma library

function text(translatedText) {
    // Keep upstream proper nouns such as "Moonlight Internet Hosting Tool"
    // intact while applying this client's short product name in translated UI.
    return translatedText.replace(/Moonlight(?! Internet Hosting Tool)/g, "Moonlight V+")
}
