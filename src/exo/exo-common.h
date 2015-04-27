#ifndef __EXO_COMMON_H__
#define __EXO_COMMON_H__

G_BEGIN_DECLS

#if defined(G_PARAM_STATIC_NAME) && defined(G_PARAM_STATIC_NICK) && defined(G_PARAM_STATIC_BLURB)
#define EXO_PARAM_READABLE  (G_PARAM_READABLE \
                           | G_PARAM_STATIC_NAME \
                           | G_PARAM_STATIC_NICK \
                           | G_PARAM_STATIC_BLURB)
#define EXO_PARAM_WRITABLE  (G_PARAM_WRITABLE \
                           | G_PARAM_STATIC_NAME \
                           | G_PARAM_STATIC_NICK \
                           | G_PARAM_STATIC_BLURB)
#define EXO_PARAM_READWRITE (G_PARAM_READWRITE \
                           | G_PARAM_STATIC_NAME \
                           | G_PARAM_STATIC_NICK \
                           | G_PARAM_STATIC_BLURB)
#else
#define EXO_PARAM_READABLE  (G_PARAM_READABLE)
#define EXO_PARAM_WRITABLE  (G_PARAM_WRITABLE)
#define EXO_PARAM_READWRITE (G_PARAM_READWRITE)
#endif

G_END_DECLS

#endif /* !__EXO_COMMON_H__ */
