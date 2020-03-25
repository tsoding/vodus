struct Gif_Animat
{
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

        // TODO(#29): If dt is too big Gif_Animat::index could probably go out of sync
        delay_time -= dt * 100;
        if (delay_time <= 0.0f) {
            index = (index + 1) % file->ImageCount;
            int ok = DGifSavedExtensionToGCB(file, index, &gcb);
            assert(ok);
            delay_time = gcb.DelayTime;
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

const size_t BTTV_MAPPING_CAPACITY = 1024;

struct Bttv_Mapping
{
    String_View name;
    String_View file;
    Maybe<Emote> emote;
};

String_View file_extension(String_View filename)
{
    String_View ext = {};
    while (filename.count != 0) {
        ext = filename.chop_by_delim('.');
    }
    return ext;
}

Emote load_gif_emote(String_View filepath)
{
    Emote emote = {Emote::Gif};

    auto filepath_cstr = string_view_as_cstr(filepath);
    defer(delete[] filepath_cstr);

    int error = 0;
    emote.gif.file = DGifOpenFileName(filepath_cstr, &error);
    if (error) {
        println(stderr, "Could not read gif file: ", filepath);
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

struct Bttv
{
    Maybe<Emote> emote_by_name(String_View name,
                               const char *channel = nullptr)
    {
        auto maybe_mapping_index = mapping_by_name(name);
        if (!maybe_mapping_index.has_value) return {};
        auto mapping_index = maybe_mapping_index.unwrap;

        if (!bttv_mapping[mapping_index].emote.has_value) {
            auto filename = bttv_mapping[mapping_index].file;
            auto ext = file_extension(filename);

            if (ext == "gif"_sv) {
                bttv_mapping[mapping_index].emote = {true, load_gif_emote(filename)};
            } else if (ext == "png"_sv) {
                bttv_mapping[mapping_index].emote = {true, load_png_emote(filename)};
            } else {
                println(stderr, filename, " has unsupported extension ", ext);
                abort();
            }
        }

        return bttv_mapping[mapping_index].emote;
    }

    void update(float delta_time)
    {
        for (size_t i = 0; i < bttv_mapping_count; ++i) {
            if (!bttv_mapping[i].emote.has_value) continue;
            if (bttv_mapping[i].emote.unwrap.type != Emote::Gif) continue;
            bttv_mapping[i].emote.unwrap.gif.update(delta_time);
        }
    }

    Maybe<size_t> mapping_by_name(String_View name)
    {
        for (size_t i = 0; i < bttv_mapping_count; ++i) {
            if (bttv_mapping[i].name == name) {
                return {true, i};
            }
        }

        return {};
    }

    void add_mapping(String_View name, String_View file)
    {
        bttv_mapping[bttv_mapping_count++] = {name, file};
    }

    Bttv_Mapping bttv_mapping[BTTV_MAPPING_CAPACITY] = {};
    size_t bttv_mapping_count = 0;
};
