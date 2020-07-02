struct Gif_Animat
{
    String_View file_path;
    GifFileType *file;
    size_t index;
    GraphicsControlBlock gcb;
    float delay_time;

    bool is_null() const
    {
        return file == nullptr;
    }

    void update(float dt)
    {
        if (is_null()) return;

        delay_time -= dt * 100;
        while (delay_time <= 0.0f) {
            index = (index + 1) % file->ImageCount;
            int ok = DGifSavedExtensionToGCB(file, index, &gcb);
            // if (!ok) {
            //     println(stderr, "[ERROR] Could not retrieve Graphics Control Block from `", file_path, "`");
            //     abort();
            // }
            delay_time = gcb.DelayTime + delay_time;
        }
    }

    void slap_onto_image32(Image32 surface, int x, int y)
    {
        if (is_null()) return;

        slap_savedimage_onto_image32(
                surface,
                &file->SavedImages[index],
                file->SColorMap,
                gcb,
                x, y);
    }

    void slap_onto_image32(Image32 surface, int x, int y, int w, int h)
    {
        if (is_null()) return;
        slap_savedimage_onto_image32(
                surface,
                &file->SavedImages[index],
                file->SColorMap,
                gcb,
                x, y,
                w, h);
    }

    int width() const
    {
        return is_null() ? 0 : file->SavedImages[index].ImageDesc.Width;
    }

    int height() const
    {
        return is_null() ? 0 : file->SavedImages[index].ImageDesc.Height;
    }
};

struct Emote
{
    enum Type
    {
        Png = 0,
        Gif
    };

    Type type;

    union
    {
        Image32 png;
        Gif_Animat gif;
    };

    int width() const
    {
        switch (type) {
        case Png: return png.width;
        case Gif: return gif.width();
        }
        assert(!"Incorrect Emote_Type value");
        return 0;
    }

    int height() const
    {
        switch (type) {
        case Png: return png.height;
        case Gif: return gif.height();
        }
        assert(!"Incorrect Emote_Type value");
        return 0;
    }

    void slap_onto_image32(Image32 surface, int x, int y, int w, int h)
    {
        switch (type) {
        case Png: {
            slap_image32_onto_image32(surface, png, x, y, w, h);
        } return;

        case Gif: {
            gif.slap_onto_image32(surface, x, y, w, h);
        } return;
        }
        assert(!"Incorrect Emote_Type value");
    }
};

const size_t EMOTE_MAPPING_CAPACITY = 1021;
const size_t EMOTE_GIFS_CAPACITY = EMOTE_MAPPING_CAPACITY;

String_View file_extension(String_View filename)
{
    String_View ext = {};
    while (filename.count != 0) {
        ext = filename.chop_by_delim('.');
    }
    return ext;
}

Emote load_gif_emote(String_View file_path)
{
    Emote emote = {Emote::Gif};

    auto file_path_cstr = string_view_as_cstr(file_path);
    defer(delete[] file_path_cstr);

    int error = 0;
    emote.gif.file_path = file_path;
    emote.gif.file = DGifOpenFileName(file_path_cstr, &error);
    if (error) {
        println(stderr, "Could not read gif file: ", file_path);
        exit(1);
    }
    assert(error == 0);
    DGifSlurp(emote.gif.file);

    return emote;
}

Emote load_png_emote(String_View filepath)
{
    Emote emote = {Emote::Png};
    auto filepath_cstr = string_view_as_cstr(filepath);
    defer(delete[] filepath_cstr);
    emote.png = load_image32_from_png(filepath_cstr);
    return emote;
}

struct Emote_Mapping
{
    String_View name;
    Emote emote;
};

struct Emote_Cache
{
    Maybe<Emote> emote_by_name(String_View name,
                               const char *channel = nullptr)
    {
        size_t i = djb2(name) % EMOTE_MAPPING_CAPACITY;
        while (emote_mapping[i].name != name && emote_mapping[i].name.count != 0) {
            i = (i + 1) % EMOTE_MAPPING_CAPACITY;
        }

        if (emote_mapping[i].name.count != 0) {
            return {true, emote_mapping[i].emote};
        }

        return {};
    }

    void update_gifs(float delta_time)
    {
        for (size_t i = 0; i < gifs_count; ++i) {
            gifs[i]->update(delta_time);
        }
    }

    void populate_from_file(const char *mapping_filepath)
    {
        auto mapping_csv = file_as_string_view(mapping_filepath);
        while (mapping_csv.count > 0 && emote_mapping_count < EMOTE_MAPPING_CAPACITY) {
            auto line = mapping_csv.chop_by_delim('\n');
            auto name = line.chop_by_delim(',');
            auto filename = line;
            auto ext = file_extension(filename);

            size_t i = djb2(name) % EMOTE_MAPPING_CAPACITY;
            while (emote_mapping[i].name.count != 0) {
                i = (i + 1) % EMOTE_MAPPING_CAPACITY;
            }

            if (ext == "gif"_sv) {
                emote_mapping[i].emote = load_gif_emote(filename);
                gifs[gifs_count++] = &emote_mapping[i].emote.gif;
            } else if (ext == "png"_sv) {
                emote_mapping[i].emote = load_png_emote(filename);
            } else {
                println(stderr, filename, " has unsupported extension ", ext);
                abort();
            }
            emote_mapping[i].name = name;
            emote_mapping_count += 1;
        }
    }

    Emote_Mapping emote_mapping[EMOTE_MAPPING_CAPACITY] = {};
    size_t emote_mapping_count = 0;
    Gif_Animat *gifs[EMOTE_GIFS_CAPACITY] = {};
    size_t gifs_count = 0;
};
