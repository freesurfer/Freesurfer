#include "realm.h"

/**
 * @file  realm.c
 * @brief support quickly scanning all the vertices or faces on an MRI for
 *        for those that might intersect a brick
 *
 * 
 */
/*
 * Original Author: Bevin R Brett
 * CVS Revision Info:
 *    $Author: brbrett $
 *    $Date: 2018/02/21 15:00:00 $
 *    $Revision: 1.0 $
 *
 * Copyright © 2018 The General Hospital Corporation (Boston, MA) "MGH"
 *
 * Terms and conditions for use, reproduction, distribution and contribution
 * are found in the 'FreeSurfer Software License Agreement' contained
 * in the file 'LICENSE' found in the FreeSurfer distribution, and here:
 *
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferSoftwareLicense
 *
 * Reporting: freesurfer@nmr.mgh.harvard.edu
 *
 */

/*-----------------------------------------------------
                    INCLUDE FILES
-------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>

#include "fnv_hash.h"


#ifdef REALM_UNIT_TEST
    //
    // rm -f a.out ; gcc -o a.out -I ../include realm.c ; ./a.out
    //
    typedef struct VERTEX {
        float x,y,z;
    } VERTEX;

    struct MRIS {
      int     nvertices;
      VERTEX* vertices;
    };
    
    static float MIN(float lhs, float rhs) { return (lhs < rhs) ? lhs : rhs; }
    static float MAX(float lhs, float rhs) { return (lhs > rhs) ? lhs : rhs; }
    
    static void bevins_break()
    {
    }

    int main() {
        MRIS mris;
        mris.nvertices = 10000;
        mris.vertices = (VERTEX*)calloc(mris.nvertices, sizeof(VERTEX));
        
        
        int vno;
        for (vno = 0; vno < mris.nvertices; vno++) {
            VERTEX* v = &mris.vertices[vno];
            v->x = vno; 
            v->y = (vno*7321)%71; 
            v->z = (vno*17321)%91;
        }
        vno = 0;
        float xMin = mris.vertices[vno].x, xMax = xMin,
              yMin = mris.vertices[vno].y, yMax = yMin, 
              zMin = mris.vertices[vno].z, zMax = zMin;
        for (vno = 1; vno < mris.nvertices; vno++) {
            VERTEX* v = &mris.vertices[vno];
            xMin = MIN(xMin, mris.vertices[vno].x);
            yMin = MIN(yMin, mris.vertices[vno].y);
            zMin = MIN(zMin, mris.vertices[vno].z);
            xMax = MAX(xMax, mris.vertices[vno].x);
            yMax = MAX(yMax, mris.vertices[vno].y);
            zMax = MAX(zMax, mris.vertices[vno].z);
        }
        
        RealmTree* realmTree = makeRealmTree(&mris);

        float fLo, fHi;
        for (fLo = 0; fLo <= 1; fLo += 0.1)
        for (fHi = 0; fHi <= 1; fHi += 0.1)
        {
            float xLo = xMin + fLo*(xMax-xMin);
            float xHi = xMax - fHi*(xMax-xLo);
            float yLo = yMin + fLo*(yMax-yMin);
            float yHi = yMax - fHi*(yMax-yLo);
            float zLo = zMin + fLo*(zMax-zMin);
            float zHi = zMax - fHi*(zMax-zLo);

            Realm* realm = 
                makeRealm(realmTree, 
                    xLo, xHi, 
                    yLo, yHi,
                    zLo, zHi);
        
            RealmIterator realmIterator;
            initRealmIterator(&realmIterator, realm);
            
            //
            
            //
            int vno;
            while (0 <= (vno = realmNextMightTouchVno(realm, &realmIterator))) {
                printf("vno:%d\n", vno);
            }
        
            freeRealm(&realm);
        }
            
        freeRealmTree(&realmTree);
        
        return 0;
    }
#endif



// RealmTree construction and destruction
//
typedef struct RealmTreeNode RealmTreeNode;
struct RealmTreeNode {
    float xLo, xHi, yLo, yHi, zLo, zHi;
    RealmTreeNode* parent;
    
#define VNOS_CAPACITY 8
    int vnoSize;
    union {
        RealmTreeNode* children[VNOS_CAPACITY];     // 2x2x2, when vnoSize  > VNOS_CAPACITY
        int            vnos    [VNOS_CAPACITY];     //        when vnoSize <= VNOS_CAPACITY
    };
};

static bool nodeContains(
    RealmTreeNode const *  const n, 
    float const x, float const y, float const z) {
    return  n->xLo <= x && x < n->xHi &&
            n->yLo <= y && y < n->yHi &&
            n->zLo <= z && z < n->zHi;
}

static RealmTreeNode const * upUntilContainsNode(RealmTreeNode const * n, 
    float const x, float const y, float const z) {
    while (!nodeContains(n,x,y,z)) n = n->parent;
    return n;
}

static int chooseChild(
    RealmTreeNode const * n,
    float x, float y,float z) {

    if (!nodeContains(n, x,y,z)) 
        bevins_break();

    float xMid = n->children[1]->xLo;
    float yMid = n->children[2]->yLo;
    float zMid = n->children[4]->zLo;
    
    int c = ((x < xMid) ? 0 : 1) + ((y < yMid) ? 0 : 2) + ((z < zMid) ? 0 : 4);
    
    if (!nodeContains(n->children[c], x,y,z)) 
        bevins_break();
    
    return c;
}

static RealmTreeNode const * deepestContainingNode(RealmTreeNode const * n, 
    float const x, float const y, float const z) {
    n = upUntilContainsNode(n, x, y, z);
    while (n->vnoSize > VNOS_CAPACITY) {
        int c = chooseChild(n, x, y, z);
        n = n->children[c];
    }
    return n;
}

static RealmTreeNode* insertIntoNode(
    RealmTree*      const realmTree,
    RealmTreeNode*  const n, 
    int const vno, float const x, float const y, float const z);

static RealmTreeNode* insertIntoChild(
    RealmTree*     realmTree,
    RealmTreeNode* n,
    int vno, float x, float y,float z) {
    int c = chooseChild(n, x, y, z);
    static int depth;
    depth++;
    if (depth > 10) {
        bevins_break();
    }
    RealmTreeNode* result = insertIntoNode(realmTree, n->children[c], vno,x,y,z);
    depth--;
    return result;
}

struct RealmTree {
    MRIS const  *   mris;
    unsigned long   fnv_hash;
    RealmTreeNode   root;
    RealmTreeNode** vnoToRealmTreeNode;
};

static RealmTreeNode* insertIntoNode(
    RealmTree*      const realmTree,
    RealmTreeNode*  const n, 
    int const vno, float const x, float const y, float const z)
{
    MRIS const* mris = realmTree->mris;

    VERTEX const* v = &mris->vertices[vno];
    if (x != v->x || y != v->y || z != v->z) 
            bevins_break();
   
    
    // Can fit in this node
    if (n->vnoSize < VNOS_CAPACITY) {
    
        if (!nodeContains(n, x,y,z)) 
            bevins_break();
   
        n->vnos[n->vnoSize++] = vno;
        realmTree->vnoToRealmTreeNode[vno] = n;
        return n;
    }

    // Has already or need to subdivide
    if (n->vnoSize == VNOS_CAPACITY) {

        // Chose the splitting values
        float xMid = (n->xLo + n->xHi)/2;
        float yMid = (n->yLo + n->yHi)/2;
        float zMid = (n->zLo + n->zHi)/2;

        // Save the vnos, since the next step overwrites them
        //
        int vnos[VNOS_CAPACITY];
        int vi;
        for (vi = 0; vi < VNOS_CAPACITY; vi++) {
            vnos[vi] = n->vnos[vi];
        } 
        
        // Make the children
        int c;
        for (c = 0; c < VNOS_CAPACITY; c++) { 
            RealmTreeNode* child = n->children[c] = 
                (RealmTreeNode*)calloc(1, sizeof(RealmTreeNode));
            child->parent = n;
            if (c&1) child->xLo = xMid, child->xHi = n->xHi; else child->xLo = n->xLo, child->xHi = xMid; 
            if (c&2) child->yLo = yMid, child->yHi = n->yHi; else child->yLo = n->yLo, child->yHi = yMid;
            if (c&4) child->zLo = zMid, child->zHi = n->zHi; else child->zLo = n->zLo, child->zHi = zMid;
        }
        n->vnoSize = VNOS_CAPACITY + 1;
        
        // Insert the saved vno into their right child
        for (vi = 0; vi < VNOS_CAPACITY; vi++) {
            VERTEX const * vertex = &mris->vertices[vnos[vi]];
            insertIntoChild(realmTree, n, vnos[vi], vertex->x, vertex->y, vertex->z);
        }
    }

    // Insert this vno into the right child
    return insertIntoChild(realmTree, n, vno, x, y, z);
}

unsigned long computeRealmTreeHash(MRIS const * mris) {
    unsigned long hash = fnv_init();
    int vno;
    for (vno = 1; vno < mris->nvertices; vno++) {
        VERTEX const * vertex = &mris->vertices[vno];
        float f[3]; f[0] = vertex->x, f[1] = vertex->y, f[2] = vertex->z;
        hash = fnv_add(hash, (const unsigned char*)f, sizeof(f));
    }
    return hash;
}


void freeRealmTree(RealmTree** realmTreePtr) {
    RealmTree* rt = *realmTreePtr; *realmTreePtr = NULL;
    if (!rt) return;
    free(rt->vnoToRealmTreeNode);
    free(rt);
}

RealmTree* makeRealmTree(MRIS const * mris) {
    // Fills in the tree using the existing position of 
    // the vertices and faces
    RealmTree* rt = (RealmTree*)calloc(1, sizeof(RealmTree));
    rt->mris     = mris;
    rt->fnv_hash = computeRealmTreeHash(mris);
    rt->vnoToRealmTreeNode = (RealmTreeNode**)calloc(mris->nvertices, sizeof(RealmTreeNode*));

    if (mris->nvertices == 0) return rt;
    
    // Calculate the outer box
    //
    int vno = 0;
    VERTEX const * vertex0 = &mris->vertices[vno];
    float xLo = vertex0->x, xHi = xLo,
          yLo = vertex0->y, yHi = yLo,
          zLo = vertex0->z, zHi = zHi;
    for (vno = 1; vno < mris->nvertices; vno++) {
        VERTEX const * vertex = &mris->vertices[vno];
        float const x = vertex->x, y = vertex->y, z = vertex->z;
        xLo = MIN(xLo, x); yLo = MIN(yLo, y); zLo = MIN(zLo, z); 
        xHi = MAX(xHi, x); yHi = MAX(yHi, y); zHi = MAX(zHi, z); 
    }
    
    // Since contains is xLo <= x < xHi etc. the bounds need to be slightly wider than Hi
    // so that the Hi is in a Node
    //
    xHi += (xHi - xLo)/100.0;
    yHi += (yHi - yLo)/100.0;
    zHi += (zHi - zLo)/100.0;
    bevins_break();
    
    RealmTreeNode* recentNode  = &rt->root;
    recentNode->xLo = xLo; recentNode->yLo = yLo; recentNode->zLo = zLo;
    recentNode->xHi = xHi; recentNode->yHi = yHi; recentNode->zHi = zHi;

    // Place all the vertices into boxes
    // 
    for (vno = 0; vno < mris->nvertices; vno++) {
        if (vno == 62)
            bevins_break();
        VERTEX const * vertex = &mris->vertices[vno];
        int const x = vertex->x, y = vertex->y, z = vertex->z;
        // Find the right subtree
        while (!nodeContains(recentNode, x,y,z)) {
            recentNode = recentNode->parent;
        }
        // Insert here, or deeper
        recentNode = insertIntoNode(rt, recentNode, vno, x,y,z);
    }

    return rt;
}

void checkRealmTree(RealmTree* realTree, MRIS const * mris) {
    unsigned long hash_now = computeRealmTreeHash(mris);
    if (realTree->fnv_hash != hash_now) {
        fprintf(stderr, "%s:%d mris some vertex xyz has changed\n", __FILE__, __LINE__);
        exit(1);
    }
}

// Realm construction and destruction
//
struct Realm {
    RealmTree     const * realmTree;
    RealmTreeNode const * deepestContainingNode;
    float                 xLo, xHi, yLo, yHi, zLo, zHi;
};

void freeRealm(Realm** realmPtr) {
    free(*realmPtr);
    *realmPtr = NULL;
}

Realm* makeRealm(
    RealmTree const * realmTree, 
    float xLo, float xHi, 
    float yLo, float yHi,
    float zLo, float zHi) {
    //
    // Creates a realm that can be used to quickly find vertices and faces
    // that MIGHT intersect this brick

    Realm* r = (Realm*)calloc(1, sizeof(Realm));
    r->realmTree = realmTree;
    r->xLo = xLo; r->yLo = yLo; r->zLo = zLo;
    r->xHi = xHi; r->yHi = yHi; r->zHi = zHi;
    
    r->deepestContainingNode = 
        upUntilContainsNode(
            deepestContainingNode(&realmTree->root, xLo, yLo, zLo), 
            xHi, yHi, zHi);
}


// Quick tests
//
bool realmMightTouchFno(Realm const * realm, int fno) {
    fprintf(stderr, "%s:%d NYI\n", __FILE__, __LINE__);
    exit(1);
    return false;
}

bool realmMightTouchVno(Realm const * realm, int vno) {
    RealmTreeNode const * realmNode = realm->deepestContainingNode;
    RealmTreeNode const * vnoNode   = realm->realmTree->vnoToRealmTreeNode[vno];
    while (vnoNode) {
        if (vnoNode == realmNode) return true;
        vnoNode = vnoNode->parent;
    }
    return false;
}
  
// Iterators
// first call with realmIterator 0
// successive calls return some next fno or vno, may not be ascending order!
// updates realmIterator to some private non-zero value
// returns -1 when no more found
// further calls will cause an error exit
// 
void initRealmIterator(RealmIterator* realmIterator, Realm* realm) {
    // The iterator implements a depth-first walk of the tree
    // It has to keep track of how far below the deepestContainingNode it is, and which of the children it is processing
    //
    // When i becomes 1, it is because we have popped the realm->deepestContainingNode
    //
    unsigned long i = 1*VNOS_CAPACITY;
    RealmTreeNode const * n = realm->deepestContainingNode;

    // Down to the deepest leftmost descendent
    //
    while (n->vnoSize > VNOS_CAPACITY) {
        n = n->children[0];
        i = i*VNOS_CAPACITY;
    }

    // Set up to access the next of these vno's, if any
    //    
    realmIterator->i = (n->vnoSize > 0) ?        i :           1;
    realmIterator->p = (n->vnoSize > 0) ? (void*)n : (void*)NULL;
}

int realmNextMightTouchFno(Realm* realm, RealmIterator* realmIterator) {
    fprintf(stderr, "%s:%d NYI\n", __FILE__, __LINE__);
    exit(1);
    return 0;
}

int realmNextMightTouchVno(Realm* realm, RealmIterator* realmIterator) {

    unsigned long         i =                        realmIterator->i;
    RealmTreeNode const * n = (RealmTreeNode const *)realmIterator->p;

    // If there isn't any more
    //
    if (!n) {
        return -1;
    }

    // Get this one
    //
    unsigned long c = i % VNOS_CAPACITY;
    int const vno = n->vnos[c];

    // Step to the next one
    //    
    c++;

    if (c < n->vnoSize) {
        
        realmIterator->i++;
        
    } else {
        
        // This leaf is consumed, so search for the next non-empty leaf
        
        // Go up thru the non-leaf's until there is an unprocessed child
        //
        do {
            n = n->parent;
            i /= VNOS_CAPACITY;
            c = (i % VNOS_CAPACITY) + 1;
            if (i == 1) {               // none
                n = NULL;
                break;
            }
        } while (c >= n->vnoSize);

        // Go down to the next unprocessed leaf
        //
        if (n) {
            do {
                n = n->children[c];
                i = i*VNOS_CAPACITY;
                c = 0;
            } while (n->vnoSize > VNOS_CAPACITY);
        }
        
        // Note for next time
        //
        realmIterator->i = i;
        realmIterator->p = (void*)n;
    }
    
    // Return this one
    //
    return vno;
}
