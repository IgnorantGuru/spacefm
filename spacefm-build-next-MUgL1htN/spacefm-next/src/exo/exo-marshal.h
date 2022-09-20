
#ifndef ___exo_marshal_MARSHAL_H__
#define ___exo_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:INT,INT (exo-marshal.list:1) */
extern void _exo_marshal_VOID__INT_INT (GClosure     *closure,
                                        GValue       *return_value,
                                        guint         n_param_values,
                                        const GValue *param_values,
                                        gpointer      invocation_hint,
                                        gpointer      marshal_data);

/* VOID:OBJECT,OBJECT (exo-marshal.list:2) */
extern void _exo_marshal_VOID__OBJECT_OBJECT (GClosure     *closure,
                                              GValue       *return_value,
                                              guint         n_param_values,
                                              const GValue *param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data);

/* STRING:POINTER (exo-marshal.list:3) */
extern void _exo_marshal_STRING__POINTER (GClosure     *closure,
                                          GValue       *return_value,
                                          guint         n_param_values,
                                          const GValue *param_values,
                                          gpointer      invocation_hint,
                                          gpointer      marshal_data);

/* STRING:STRING,STRING (exo-marshal.list:4) */
extern void _exo_marshal_STRING__STRING_STRING (GClosure     *closure,
                                                GValue       *return_value,
                                                guint         n_param_values,
                                                const GValue *param_values,
                                                gpointer      invocation_hint,
                                                gpointer      marshal_data);

/* BOOLEAN:VOID (exo-marshal.list:5) */
extern void _exo_marshal_BOOLEAN__VOID (GClosure     *closure,
                                        GValue       *return_value,
                                        guint         n_param_values,
                                        const GValue *param_values,
                                        gpointer      invocation_hint,
                                        gpointer      marshal_data);

/* BOOLEAN:ENUM,INT (exo-marshal.list:6) */
extern void _exo_marshal_BOOLEAN__ENUM_INT (GClosure     *closure,
                                            GValue       *return_value,
                                            guint         n_param_values,
                                            const GValue *param_values,
                                            gpointer      invocation_hint,
                                            gpointer      marshal_data);

/* BOOLEAN:INT,ENUM,BOOLEAN,ENUM,BOOLEAN (exo-marshal.list:7) */
extern void _exo_marshal_BOOLEAN__INT_ENUM_BOOLEAN_ENUM_BOOLEAN (GClosure     *closure,
                                                                 GValue       *return_value,
                                                                 guint         n_param_values,
                                                                 const GValue *param_values,
                                                                 gpointer      invocation_hint,
                                                                 gpointer      marshal_data);

G_END_DECLS

#endif /* ___exo_marshal_MARSHAL_H__ */

