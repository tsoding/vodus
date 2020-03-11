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

enum class Bttv_Emote_Type
{
    Png = 0,
    Gif
};

struct Bttv_Emote
{
    Bttv_Emote_Type type;
    union
    {
        Image32 png;
        Gif_Animat gif;
    };
};

template <typename T>
struct Maybe
{
    bool has_value;
    T unwrap;
};

struct Bttv
{
    Maybe<Bttv_Emote> emote_by_name(String_View name,
                                    const char *channel = nullptr)
    {
        // TODO(#19): Emotes in Bttv::emote_by_name are hardcoded
        //    Some sort of a cache system is required here.
        if (name == "AYAYA"_sv) {
            Bttv_Emote emote = {Bttv_Emote_Type::Png};
            emote.png = ayaya_image;
            return {true, emote};
        } else if (name == "phpHop"_sv) {
            Bttv_Emote emote = {Bttv_Emote_Type::Gif};
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
