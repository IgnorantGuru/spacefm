#ifndef __EXO_COMMON_H__
#define __EXO_COMMON_H__

/* Taken from exo v0.10.2 (Debian package libexo-1-0), according to changelog
 * commit f455681554ca205ffe49bd616310b19f5f9f8ef1 Dec 27 13:50:21 2012 */

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
