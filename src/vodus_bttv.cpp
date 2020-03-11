struct Gif_Animat
{
    GifFileType *file;
    size_t index;
    GraphicsControlBlock gcb;
    float delay_time;

    void update(float dt)
    {
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
        slap_savedimage_onto_image32(
            surface,
            &file->SavedImages[index],
            file->SColorMap,
            gcb,
            x, y);
    }

    void slap_onto_image32(Image32 surface, int x, int y, int w, int h)
    {
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
        return file->SavedImages[index].ImageDesc.Width;
    }

    int height() const
    {
        return file->SavedImages[index].ImageDesc.Height;
    }
};

enum class Emote_Type
{
    Png = 0,
    Gif
};

struct Emote
{
    Emote_Type type;
    union
    {
        Image32 png;
        Gif_Animat gif;
    };

    int width() const
    {
        switch (type) {
        case Emote_Type::Png: return png.width;
        case Emote_Type::Gif: return gif.width();
        }
        assert(!"Incorrect Emote_Type value");
        return 0;
    }

    int height() const
    {
        switch (type) {
        case Emote_Type::Png: return png.height;
        case Emote_Type::Gif: return gif.height();
        }
        assert(!"Incorrect Emote_Type value");
        return 0;
    }

    void slap_onto_image32(Image32 surface, int x, int y, int w, int h)
    {
        switch (type) {
        case Emote_Type::Png: {
            slap_image32_onto_image32(surface, png, x, y, w, h);
        } return;

        case Emote_Type::Gif: {
            gif.slap_onto_image32(surface, x, y, w, h);
        } return;
        }
        assert(!"Incorrect Emote_Type value");
    }
};

template <typename T>
struct Maybe
{
    bool has_value;
    T unwrap;
};

struct Bttv
{
    Maybe<Emote> emote_by_name(String_View name,
                                    const char *channel = nullptr)
    {
        // TODO(#19): Emotes in Bttv::emote_by_name are hardcoded
        //    Some sort of a cache system is required here.
        if (name == "AYAYA"_sv) {
            Emote emote = {Emote_Type::Png};
            emote.png = ayaya_image;
            return {true, emote};
        } else if (name == "phpHop"_sv) {
            Emote emote = {Emote_Type::Gif};
            emote.gif = php_hop;
            return {true, emote};
        }

        return {};
    }

    void update(float delta_time)
    {
        php_hop.update(delta_time);
    }

    Image32 ayaya_image;
    Gif_Animat php_hop;
};
