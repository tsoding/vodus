struct Gif_Animat
{
    String_View file_path;
    Animat32 animat;
    size_t index;
    float delay_time;

    bool is_null() const
    {
        return animat.count == 0;
    }

    void update(float dt)
    {
        if (!is_null()) {
            delay_time -= dt * 100;
            while (delay_time <= 0.0f) {
                index = (index + 1) % animat.count;
                delay_time = animat.frame_delays[index] + delay_time;
            }
        }
    }

    void slap_onto_image32(Image32 surface, int x, int y)
    {
        if (!is_null()) {
            slap_image32_onto_image32(surface, animat.frames[index], x, y);
        }
    }

    int width() const
    {
        return is_null() ? 0 : animat.frames[index].width;
    }

    int height() const
    {
        return is_null() ? 0 : animat.frames[index].height;
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

    void slap_onto_image32(Image32 surface, int x, int y)
    {
        switch (type) {
        case Png: {
            slap_image32_onto_image32(surface, png, x, y);
        } return;

        case Gif: {
            gif.slap_onto_image32(surface, x, y);
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


Emote load_gif_emote(String_View file_path, size_t size)
{
    Emote emote = {Emote::Gif};

    auto file_path_cstr = string_view_as_cstr(file_path);
    assert(file_path_cstr);
    defer(free((void*) file_path_cstr));

    emote.gif.animat = load_animat32_from_gif(file_path_cstr, size);
    emote.gif.file_path = file_path;

    return emote;
}

Emote load_png_emote(String_View filepath, size_t size)
{
    Emote emote = {Emote::Png};
    auto filepath_cstr = string_view_as_cstr(filepath);

    assert(filepath_cstr);
    defer(free((void*) filepath_cstr));

    emote.png = load_image32_from_png(filepath_cstr, size);
    return emote;
}

Emote load_emote(String_View filepath, size_t size)
{
    auto ext = file_extension(filepath);
    if (ext == "gif"_sv) {
        return load_gif_emote(filepath, size);
    } else if (ext == "png"_sv) {
        return load_png_emote(filepath, size);
    } else {
        println(stderr, filepath, " has unsupported extension ", ext);
        abort();
    }

    return {};
}

struct Emote_Mapping
{
    String_View name;
    Maybe<Emote> emote;
    String_View filepath;
};

// NOTE: stolen from http://www.cse.yorku.ca/~oz/hash.html
unsigned long djb2(String_View str)
{
    unsigned long hash = 5381;
    for (size_t i = 0; i < str.count; ++i) {
        hash = ((hash << 5) + hash) + str.data[i];
    }
    return hash;
}

// TODO(#109): pajaWalk emotes are not render correctly (something wrong with the palette)
// TODO: Some gif emote combos can be out of sync because of the lazy loading
struct Emote_Cache
{
    Maybe<Emote> emote_by_name(String_View name, size_t size)
    {
        size_t i = djb2(name) % EMOTE_MAPPING_CAPACITY;
        while (emote_mapping[i].name != name && emote_mapping[i].name.count != 0) {
            i = (i + 1) % EMOTE_MAPPING_CAPACITY;
        }

        if (emote_mapping[i].name.count != 0) {
            if (!emote_mapping[i].emote.has_value) {
                emote_mapping[i].emote = {true, load_emote(emote_mapping[i].filepath, size)};
                if (emote_mapping[i].emote.unwrap.type == Emote::Gif) {
                    gifs[gifs_count++] = &emote_mapping[i].emote.unwrap.gif;
                }
            }

            return emote_mapping[i].emote;
        }

        return {};
    }

    void update_gifs(float delta_time)
    {
        for (size_t i = 0; i < gifs_count; ++i) {
            gifs[i]->update(delta_time);
        }
    }

    void populate_from_file(const char *mapping_filepath, size_t size)
    {
        auto mapping_csv = read_file_as_string_view(mapping_filepath);
        if (!mapping_csv.has_value) {
            println(stderr, "Could not read file `", mapping_filepath, "`");
            abort();
        }

        while (mapping_csv.unwrap.count > 0 && emote_mapping_count < EMOTE_MAPPING_CAPACITY) {
            auto line = mapping_csv.unwrap.chop_by_delim('\n');
            auto name = line.chop_by_delim(',');
            auto filename = line;

            size_t i = djb2(name) % EMOTE_MAPPING_CAPACITY;
            while (emote_mapping[i].name.count != 0) {
                i = (i + 1) % EMOTE_MAPPING_CAPACITY;
            }

            emote_mapping[i].filepath = filename;
            emote_mapping[i].emote = {};
            emote_mapping[i].name = name;
            emote_mapping_count += 1;
        }
    }

    Emote_Mapping emote_mapping[EMOTE_MAPPING_CAPACITY] = {};
    size_t emote_mapping_count = 0;
    Gif_Animat *gifs[EMOTE_GIFS_CAPACITY] = {};
    size_t gifs_count = 0;
};
