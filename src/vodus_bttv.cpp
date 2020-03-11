struct Gif_Animat
{
    GifFileType *file;
    size_t index;
    GraphicsControlBlock gcb;
    float delay_time;

    void update(float dt)
    {
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
            Emote emote = {Emote::Png};
            emote.png = ayaya_image;
            return {true, emote};
        } else if (name == "phpHop"_sv) {
            Emote emote = {Emote::Gif};
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
