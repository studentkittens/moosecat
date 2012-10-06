#ifndef MC_CLIENT_PRIVATE_HH
#define MC_CLIENT_PRIVATE_HH

#define BEGIN_COMMAND                           \
    mpd_connection * conn = mc_proto_get(self); \
    if(conn != NULL)                            \
    {                                           \
         

#define END_COMMAND                             \
    }                                           \
    mc_proto_put(self);                         \
     
/* Template to define new commands */
#define COMMAND(_code_)                         \
    BEGIN_COMMAND                               \
    {                                           \
        _code_;                                 \
    }                                           \
    END_COMMAND                                 \
     
#endif /* end of include guard: MC_CLIENT_PRIVATE_HH */
