    namespace TopologyM {
    struct Face : public MRIS_Elt {
        inline Face (                        );
        inline Face ( Face const & src       );
        inline Face ( MRIS* mris, size_t idx );
        inline Face ( AllM::Face const & src );

        inline vertices_per_face_t v        (   ) const ;
        inline char                ripflag  (   ) const ;
        inline char                oripflag (   ) const ;
        inline int                 marked   (   ) const ;
                   
        inline void set_v        (  vertices_per_face_t to ) ;
        inline void set_ripflag  (                 char to ) ;
        inline void set_oripflag (                 char to ) ;
        inline void set_marked   (                  int to ) ;
    };

    struct Vertex : public MRIS_Elt {
        inline Vertex (                          );
        inline Vertex ( Vertex const & src       );
        inline Vertex ( MRIS* mris, size_t idx   );
        inline Vertex ( AllM::Vertex const & src );

        // put the pointers before the ints, before the shorts, before uchars, to reduce size
        // the whole fits in much less than one cache line, so further ordering is no use
        inline Face   f             ( size_t i  ) const ;  // size() is num.    array[v->num] the fno's of the neighboring faces            
        inline size_t n             ( size_t i  ) const ;  // size() is num.    array[v->num] the face.v[*] index for this vertex           
        inline int    e             ( size_t i  ) const ;  //  edge state for neighboring vertices                                          
        inline Vertex v             ( size_t i  ) const ;  // size() is vtotal.    array[v->vtotal or more] of vno, head sorted by hops     
        inline short  vnum          (           ) const ;  //  number of 1-hop neighbors    should use [p]VERTEXvnum(i,                     
        inline short  v2num         (           ) const ;  //  number of 1, or 2-hop neighbors                                              
        inline short  v3num         (           ) const ;  //  number of 1,2,or 3-hop neighbors                                             
        inline short  vtotal        (           ) const ;  //  total # of neighbors. copy of vnum.nsizeCur                                  
        inline short  nsizeMaxClock (           ) const ;  //  copy of mris->nsizeMaxClock when v#num                                       
        inline uchar  nsizeMax      (           ) const ;  //  the max nsize that was used to fill in vnum etc                              
        inline uchar  nsizeCur      (           ) const ;  //  index of the current v#num in vtotal                                         
        inline uchar  num           (           ) const ;  //  number of neighboring faces                                                  
        inline char   ripflag       (           ) const ;  //  vertex no longer exists - placed last to load the next vertex into cache     
        // put the pointers before the ints, before the shorts, before uchars, to reduce size
        // the whole fits in much less than one cache line, so further ordering is no use
                   
        inline void set_f             ( size_t i,   Face to )    ;  // size() is num.    array[v->num] the fno's of the neighboring faces         
        inline void set_n             ( size_t i, size_t to )    ;  // size() is num.    array[v->num] the face.v[*] index for this vertex        
        inline void set_e             ( size_t i,    int to )                     ;  //  edge state for neighboring vertices                      
        inline void set_v             ( size_t i, Vertex to ) ;  // size() is vtotal.    array[v->vtotal or more] of vno, head sorted by hops     
        inline void set_vnum          (            short to )                     ;  //  number of 1-hop neighbors    should use [p]VERTEXvnum(i, 
        inline void set_v2num         (            short to )                     ;  //  number of 1, or 2-hop neighbors                          
        inline void set_v3num         (            short to )                     ;  //  number of 1,2,or 3-hop neighbors                         
        inline void set_vtotal        (            short to )                     ;  //  total # of neighbors. copy of vnum.nsizeCur              
        inline void set_nsizeMaxClock (            short to )                     ;  //  copy of mris->nsizeMaxClock when v#num                   
        inline void set_nsizeMax      (            uchar to )                     ;  //  the max nsize that was used to fill in vnum etc          
        inline void set_nsizeCur      (            uchar to )                     ;  //  index of the current v#num in vtotal                     
        inline void set_num           (            uchar to )                     ;  //  number of neighboring faces                              
        inline void set_ripflag       (             char to )      ;  //  vertex no longer exists - placed last to load the next vertex into cache
    };

    struct Surface : public MRIS_Elt {
        inline Surface (                           );
        inline Surface ( Surface const & src       );
        inline Surface ( MRIS* mris, size_t idx    );
        inline Surface ( AllM::Surface const & src );

        inline MRIS_fname_t fname          (   ) const ;  //  file it was originally loaded from                                       
        inline MRIS_Status  status         (   ) const ;  //  type of surface (e.g. sphere, plane)                                     
        inline MRIS_Status  origxyz_status (   ) const ;  //  type of surface (e.g. sphere, plane) that this origxyz were obtained from
        inline int          patch          (   ) const ;  //  if a patch of the surface                                                
    };

    } // namespace TopologyM
