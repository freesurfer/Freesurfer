#ifndef REALM_H
#define REALM_H

/**
 * @file  realm.h
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

#include "mris.h"

typedef struct RealmTree RealmTree;
void freeRealmTree(RealmTree** realmTreePtr);
RealmTree* makeRealmTree(MRIS const * mris);
void checkRealmTree(RealmTree* realTree, MRIS const * mris);
    //
    // Fills in the tree using the existing position of 
    // the vertices and faces.  The check version verifies
    // that the faces and vertices have not moved since they were 
    // used to make the tree.

typedef struct Realm Realm;
void freeRealm(Realm** realmPtr);
Realm* makeRealm(RealmTree const * realmTree, 
    float xLo, float xHi, 
    float yLo, float yHi,
    float zLo, float zHi);
    //
    // Creates a realm that can be used to quickly find vertices and faces
    // that MIGHT intersect this brick
    
bool realmMightTouchFno(Realm const * realm, int fno);
bool realmMightTouchVno(Realm const * realm, int vno);
    //
    // Quick tests
    
int realmNextMightTouchFno(Realm* realm, int & realmIterator);
int realmNextMightTouchVno(Realm* realm, int & realmIterator);
    // first call with realmIterator 0
    // successive calls return some next fno or vno, may not be ascending order!
    // updates realmIterator to some private non-zero value
    // returns -1 when no more found
    // further calls will cause an error exit

#endif
