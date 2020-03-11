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
        GifFileType *gif;
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
        // TODO: Emotes in Bttv::emote_by_name are hardcoded
        //    Some sort of a cache system is required here.
        if (name == "AYAYA"_sv) {
            return {true, {Bttv_Emote_Type::Png, ayaya_image}};
        }

        return {};
    }

    Image32 ayaya_image;
};
