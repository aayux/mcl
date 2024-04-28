/*******************************************************************************
* This is experimental code for Non-interactive Anonymous Tokens
* Author(s):    Quan Nguyen (@quanngu31)
*               Aayush Yadav (@aayux)
********************************************************************************
*----------------------------------- HOW TO ------------------------------------
********************************************************************************
* Follow the steps below to run this file:
*
* 1. Clone the repo: 
* $ git clone https://github.com/aayux/mcl.git
*
* 2. Make the exe
* $ make bin/niat.exe 
*
* 3. Execute the exe
* $ ./bin/niat.exe 
*******************************************************************************/

#include <mcl/bls12_381.hpp>

using namespace mcl::bn;
using std::vector;

G1 P;   // generator g1 of G1
G2 Q;   // generator g2 of G2

// pre-computed pairings
Fp12 eC, e_pkCm_pkI1m;  // Client
G1 pkC_m;
Fp12 eI, e_g1n_pkI1n;   // Issuer

struct eqsig {
    G1 Z, Y1;
    G2 Y2;
};

struct niat_token {
    G1 tag[3];
    eqsig sig;
};

struct nizkpf {
    Fr c0, c1, as, a0, a1, a2;
};

struct niat_psig {
    eqsig sig;
    G1 S, T;
    nizkpf pi;
    std::string r;
};

void EQKeyGen(Fr *sk, G2 pk[3]) {
    for (int i = 0; i < 3; i++) {
        sk[i].setRand();
        G2::mul(pk[i], Q, sk[i]);
    }
}

void HashtoG1(G1& X, const std::string& x) {
    Fp tmp;
    tmp.setHashOf(x);
    mapToG1(X, tmp);
}

void EQSign(eqsig *s, G1 m[3], const Fr sk[3]) {
    Fr nu, nu_inv;
    G1 tmp;
    nu.setRand();
    Fr::inv(nu_inv, nu);

    G1::mul(s->Y1, P, nu_inv);   // Y1 = g1^(1/r)
    G2::mul(s->Y2, Q, nu_inv);   // Y2 = g2^(1/r)

    G1::mul(s->Z, m[0], sk[0]); // Z = (m1^x1 * m2^x2 * m3^x3)^nu
    G1::mul(tmp, m[1], sk[1]);
    G1::add(s->Z, s->Z, tmp);
    G1::mul(tmp, m[2], sk[2]);
    G1::add(s->Z, s->Z, tmp);
    G1::mul(s->Z, s->Z, nu);
}

bool EQVerify(const G2 pk[3], G1 m[3], eqsig s) {
    Fp12 e1, e2, e3, e4;
    pairing(e1, m[0], pk[0]);
    pairing(e2, m[1], pk[1]);
    Fp12::mul(e2, e1, e2);
    pairing(e3, m[2], pk[2]);
    Fp12::mul(e3, e2, e3);
    pairing(e4, s.Z, s.Y2);
    pairing(e1, P, s.Y2);
    pairing(e2, s.Y1, Q);
    // e(g1, Y2) = e(Y1, g2) & e(m1, pk1) * e(m2, pk2) * e(m3, pk3) = e(Z, Y2)
    return (e1 == e2 && e3 == e4);
}

void EQChRep(eqsig *s_, eqsig s, const Fr mu) {
    // bool ok = EQVerify(pk, m, s);
    // if (!ok) {
    //     throw std::runtime_error("EQChRep: cannot verify signature.");
    // }

    Fr psi, psi_inv;
    psi.setRand();
    Fr::inv(psi_inv, psi);

    G1::mul(s_->Y1, s.Y1, psi_inv);     //Y1' = Y1^(1/psi)}
    G2::mul(s_->Y2, s.Y2, psi_inv);     //Y2' = Y2^(1/psi)}

    Fr::mul(psi, psi, mu);
    G1::mul(s_->Z, s.Z, psi);          // Z' = Z^(mu * psi)
}

void NIZKProve(const G1 pkC, const G2 pkI[3], niat_psig *psig, Fr skI[3], Fr s, int b){
    G2 W0, W1;
    W0 = pkI[1];
    G2::add(W1, pkI[0], pkI[1]);

    if (b == 0) {
        Fr rs, r0, c1, a1, a2;
        rs.setRand(); r0.setRand();
        c1.setRand(); a1.setRand(); a2.setRand();

        G1 Rs, R1, R3, tmp1; G2 R2, R4, tmp2;
        Fr minusc1;
        Fr::neg(minusc1, c1);

        G1::mul(Rs, P, rs);

        G1::mul(R1, psig->S, r0);

        G2::mul(R2, Q, r0);

        G1::mul(R3, pkC, a1);
        G1::mul(tmp1, psig->S, a2);
        G1::add(R3, R3, tmp1);
        G1::mul(tmp1, psig->T, minusc1);
        G1::add(R3, R3, tmp1);

        G2::mul(R4, Q, a1);
        G2::mul(tmp2, Q, a2);
        G2::add(R4, R4, tmp2);
        G2::mul(tmp2, W1, minusc1);
        G2::add(R4, R4, tmp2);

        std::string cstr =  "<beg> P: " + P.getStr() + "\n Q: " + Q.getStr() + 
                        "\n pkC: " + pkC.getStr() + "\n S: " + psig->S.getStr() +
                        "\n T: " + psig->T.getStr() + "\n W0: " + W0.getStr() + 
                        "\n W1: " + W1.getStr() + "\n Rs: " + Rs.getStr() + 
                        "\n R1: " + R1.getStr() + "\n R2: " + R2.getStr() + 
                        "\n R3: " + R3.getStr() + "\n R4: " + R4.getStr() + 
                        "<end>";
        Fr c;
        c.setHashOf(cstr);
        
        Fr as, c0, a0, tmp;
        Fr::mul(tmp, c, s);
        Fr::add(as, rs, tmp);       // as = rs + c * s
        Fr::add(c0, c, minusc1);    // c0 = c - c1
        Fr::mul(tmp, c0, skI[1]);
        Fr::add(a0, r0, tmp);       // a0 = r0 + c0 * x2

        psig->pi.c0 = c0;
        psig->pi.c1 = c1;
        psig->pi.as = as;
        psig->pi.a0 = a0;
        psig->pi.a1 = a1;
        psig->pi.a2 = a2;

    }
    else if (b == 1) {
        Fr rs, r1, r2, c0, a0;
        rs.setRand(); r1.setRand();
        r2.setRand(); c0.setRand(); a0.setRand();

        G1 Rs, R1, R3, tmp1; G2 R2, R4, tmp2;
        Fr minusc0;
        Fr::neg(minusc0, c0);

        G1::mul(Rs, P, rs);

        G1::mul(R1, psig->S, a0);
        G1::mul(tmp1, psig->T, minusc0);
        G1::add(R1, R1, tmp1);

        G2::mul(R2, Q, a0);
        G2::mul(tmp2, W0, minusc0);
        G2::add(R2, R2, tmp2);

        G1::mul(R3, pkC, r1);
        G1::mul(tmp1, psig->S, r2);
        G1::add(R3, R3, tmp1);

        G2::mul(R4, Q, r1);
        G2::mul(tmp2, Q, r2);
        G2::add(R4, R4, tmp2);

        std::string cstr =  "<beg> P: " + P.getStr() + "\n Q: " + Q.getStr() + 
                        "\n pkC: " + pkC.getStr() + "\n S: " + psig->S.getStr() +
                        "\n T: " + psig->T.getStr() + "\n W0: " + W0.getStr() + 
                        "\n W1: " + W1.getStr() + "\n Rs: " + Rs.getStr() + 
                        "\n R1: " + R1.getStr() + "\n R2: " + R2.getStr() + 
                        "\n R3: " + R3.getStr() + "\n R4: " + R4.getStr() + 
                        "<end>";
        Fr c;
        c.setHashOf(cstr);

        Fr as, c1, a1, a2, tmp;
        Fr::mul(tmp, c, s);
        Fr::add(as, rs, tmp);
        Fr::add(c1, c, minusc0);
        Fr::mul(tmp, c1, skI[0]);
        Fr::add(a1, r1, tmp);
        Fr::mul(tmp, c1, skI[1]);
        Fr::add(a2, r2, tmp);

        psig->pi.c0 = c0;
        psig->pi.c1 = c1;
        psig->pi.as = as;
        psig->pi.a0 = a0;
        psig->pi.a1 = a1;
        psig->pi.a2 = a2;
    }
}

bool NIZKVerify(const G1 pkC, const G2 pkI[3], niat_psig psig){
    G1 Rs, R1, R3, tmp1; G2 W0, W1, R2, R4, tmp2;
    W0 = pkI[1];
    G2::add(W1, pkI[0], pkI[1]);

    Fr c, minusc, minusc0, minusc1;
    
    Fr::add(c, psig.pi.c0, psig.pi.c1);
    Fr::neg(minusc, c);
    G1::mul(Rs, P, psig.pi.as);
    G1::mul(tmp1, psig.S, minusc);
    G1::add(Rs, Rs, tmp1);
    
    Fr::neg(minusc0, psig.pi.c0);
    G1::mul(R1, psig.S, psig.pi.a0);
    G1::mul(tmp1, psig.T, minusc0);
    G1::add(R1, R1, tmp1);
    
    G2::mul(R2, Q, psig.pi.a0);
    G2::mul(tmp2, W0, minusc0);
    G2::add(R2, R2, tmp2);

    Fr::neg(minusc1, psig.pi.c1);
    G1::mul(R3, pkC, psig.pi.a1);
    G1::mul(tmp1, psig.S, psig.pi.a2);
    G1::add(R3, R3, tmp1);
    G1::mul(tmp1, psig.T, minusc1);
    G1::add(R3, R3, tmp1);

    G2::mul(R4, Q, psig.pi.a1);
    G2::mul(tmp2, Q, psig.pi.a2);
    G2::add(R4, R4, tmp2);
    G2::mul(tmp2, W1, minusc1);
    G2::add(R4, R4, tmp2);

    std::string cstr =  "<beg> P: " + P.getStr() + "\n Q: " + Q.getStr() + 
                        "\n pkC: " + pkC.getStr() + "\n S: " + psig.S.getStr() +
                        "\n T: " + psig.T.getStr() + "\n W0: " + W0.getStr() + 
                        "\n W1: " + W1.getStr() + "\n Rs: " + Rs.getStr() + 
                        "\n R1: " + R1.getStr() + "\n R2: " + R2.getStr() + 
                        "\n R3: " + R3.getStr() + "\n R4: " + R4.getStr() + 
                        "<end>";
    Fr c_;
    c_.setHashOf(cstr);

    return (c == c_);
}


void NIATClientKeyGen(Fr& skC, G1& pkC) {
    skC.setRand();
    G1::mul(pkC, P, skC);
}

void NIATIssue(G2 pkI[3], niat_psig *psig, Fr skI[3], const G1& pkC, int b) {
    psig->r = "random r is hardcoded for now!";
    Fr s;
    s.setRand();
    G1::mul(psig->S, P, s); // S = g1^s

    G1 x2S;
    G1::mul(x2S, psig->S, skI[1]);
    
    // T = pkC^(b * x1) * S^(x2)
    if (b == 0) {
        psig->T = x2S;
    }
    else if (b == 1) {
        G1::mul(psig->T, pkC, skI[0]);
        G1::add(psig->T, psig->T, x2S);
    }

    G1 m[3];
    m[0] = pkC;
    HashtoG1(m[1], psig->r);
    m[2] = psig->T;

    EQSign(&psig->sig, m, skI);

    NIZKProve(pkC, pkI, psig, skI, s, b);
}

bool NIATClientEQVer(const G2 pk[3], G1 m[3], eqsig s) {
    Fp12 e1, e2, e3, e4;
    pairing(e2, m[1], pk[1]);
    Fp12::mul(e2, eC, e2);      // use the precomputed eC
    pairing(e3, m[2], pk[2]);
    Fp12::mul(e3, e2, e3);
    pairing(e4, s.Z, s.Y2);
    pairing(e1, P, s.Y2);
    pairing(e2, s.Y1, Q);
    // e(g1, Y2) = e(Y1, g2) & e(m1, pk1) * e(m2, pk2) * e(m3, pk3) = e(Z, Y2)
    return (e1 == e2 && e3 == e4);
}

/* Overloaded */
bool NIATClientEQVer(const G2 pkI[3], vector<vector<G1> > m, vector<eqsig> s, int numTokens) {
    G1 Y1i; G2 Y2i;
    // zero out for sum
    Y1i.clear();
    Y2i.clear();
    for (int i=0; i < numTokens; i++) {
        G1::add(Y1i, Y1i, s[i].Y1);
        G2::add(Y2i, Y2i, s[i].Y2);
    }
    Fp12 e1, e2;
    pairing(e1, Y1i, Q);
    pairing(e2, P, Y2i);
    if (e1 != e2) {     // e(g1, Y2) = e(Y1, g2), but aggregated
        return false;   // early termination if fails
    } 
    // for client
    // m[0] = Hr;
    // m[1] = psig.T;

    // e(Z, Y2) = e(m1, pk1) * e(m2, pk2) * e(m3, pk3)
    
    Fp12 temp;
    // set to one for multiplications
    e1.setOne();    // LHS: e(Z, Y2)
    e2.setOne();    // RHS: e(m1, pk1) * e(m2, pk2) * e(m3, pk3)

    for (int i=0; i < numTokens; i++) {
        pairing(temp, s[i].Z, s[i].Y2); e1 *= temp;
        pairing(temp, m[i][0], pkI[1]); e2 *= temp;
        pairing(temp, m[i][1], pkI[2]); e2 *= temp;
    }
    e2 *= e_pkCm_pkI1m; // pre-computed pairing
    return (e1 == e2);
}

bool NIATIssuerEQVer(const G2 pk[3], G1 m[3], eqsig s) {
    Fp12 e1, e2, e3, e4;
    pairing(e2, m[1], pk[1]);
    Fp12::mul(e2, eI, e2);      // use the precomputed eI
    pairing(e3, m[2], pk[2]);
    Fp12::mul(e3, e2, e3);
    pairing(e4, s.Z, s.Y2);
    pairing(e1, P, s.Y2);
    pairing(e2, s.Y1, Q);
    // e(g1, Y2) = e(Y1, g2) & e(m1, pk1) * e(m2, pk2) * e(m3, pk3) = e(Z, Y2)
    return (e1 == e2 && e3 == e4);
}

/* Overloaded */
bool NIATIssuerEQVer(const G2 pkI[3], vector<vector<G1> > m, vector<eqsig> s, int numTokens) {
    G1 Y1i; G2 Y2i;
    // zero out for sum
    G1::mul(Y1i, P, 0);
    G2::mul(Y2i, Q, 0);
    for (int i=0; i < numTokens; i++) {
        G1::add(Y1i, Y1i, s[i].Y1);
        G2::add(Y2i, Y2i, s[i].Y2);
    }
    Fp12 e1, e2;
    pairing(e1, Y1i, Q);
    pairing(e2, P, Y2i);
    if (e1 != e2) {     // e(g1, Y2) = e(Y1, g2), but aggregated
        return false;   // early termination if fails
    }
    // for issuer
    // m[0] = t.tag[0];
    // m[1] = t.tag[1];

    // e(Z, Y2) = e(g1, pk1) * e(t1, pk2) * e(t2, pk3)

    Fp12 temp;
    // set to one for multiplications
    e1.setOne();    // LHS: e(Z, Y2)
    e2.setOne();    // RHS: e(m1, pk1) * e(m2, pk2) * e(m3, pk3)

    for (int i=0; i < numTokens; i++) {
        pairing(temp, s[i].Z, s[i].Y2); e1 *= temp;

        pairing(temp, m[i][0], pkI[1]); e2 *= temp;
        pairing(temp, m[i][1], pkI[2]); e2 *= temp;
    }
    e2 *= e_g1n_pkI1n; // pre-computed pairing
    return (e1 == e2);
}

void NIATObtain(niat_token *t, const Fr skC, const G1& pkC, G2 pkI[3], niat_psig psig, bool eqVerified = false) {
    G1 m[3], Hr;
    HashtoG1(Hr, psig.r);
    m[0] = pkC;
    m[1] = Hr;
    m[2] = psig.T;

    if ( ! eqVerified ) {
        if ( ! NIATClientEQVer(pkI, m, psig.sig) ) {
            throw std::runtime_error("ERR <NIATObtain>: cannot verify presignature.");
        }
    }    
    if ( ! NIZKVerify(pkC, pkI, psig)) {
        throw std::runtime_error("ERR <NIATObtain>: cannot verify NIZK.");
    }

    Fr alpha_inv;
    Fr::inv(alpha_inv, skC);

    G1::mul(t->tag[0], Hr, alpha_inv);
    G1::mul(t->tag[1], psig.T, alpha_inv);
    G1::mul(t->tag[2], psig.S, alpha_inv);

    EQChRep(&t->sig, psig.sig, alpha_inv);
}

bool NIATClientBatchVerify(const G2 pkI[3], vector<niat_psig> psigs, int numTokens) {
    vector<vector<G1> > msgs(numTokens, vector<G1>(2)); 
    vector<eqsig> sigs(numTokens);
    
    // prep values for verification
    for (int i=0; i < numTokens; i++) {
        // all pkC's are the same in a batch
        HashtoG1(msgs[i][0], psigs[i].r);
        msgs[i][1] = psigs[i].T;
        sigs[i] = psigs[i].sig;
    }
    // verify a batch of tokens
    if ( ! NIATClientEQVer(pkI, msgs, sigs, numTokens)) {
        throw std::runtime_error("ERR <NIATClientBatchVerify>: cannot verify presignatures.");
    }
    return true;
}

bool NIATIssuerBatchVerify(const G2 pkI[3], vector<niat_token> toks, int numTokens) {
    vector<vector<G1> > msgs(numTokens, vector<G1>(2));
    vector<eqsig> sigs(numTokens);
    // prep values for verification
    for (int i=0; i < numTokens; i++) {
        // first value is always g1
        msgs[i][0] = toks[i].tag[0];
        msgs[i][1] = toks[i].tag[1];
        sigs[i] = toks[i].sig;
    }
    // verify a batch of tokens
    if ( ! NIATIssuerEQVer(pkI, msgs, sigs, numTokens)) {
        throw std::runtime_error("ERR <NIATIssuerBatchVerify>: cannot verify presignatures.");
    }
    return true;
}

int NIATReadBit(Fr skI[3], G2 pkI[3], niat_token t, bool eqVerified = false) { 
    if ( ! eqVerified ) {
        G1 m[3];
        m[0] = P;
        m[1] = t.tag[0];
        m[2] = t.tag[1];
        if ( ! NIATIssuerEQVer(pkI, m, t.sig) ) { return -1; }
    }

    G1 lhs, rhs;
    G1::mul(rhs, P, skI[0]);

    Fr minusx2;
    Fr::neg(minusx2, skI[1]);
    G1::mul(lhs, t.tag[2], minusx2);
    G1::add(lhs, t.tag[1], lhs);

    if (lhs == rhs) { return 1; }
    else if (lhs.isZero()) { return 0; }
    return -1;
}

int main() {
    // setup global parameters
    initPairing(mcl::BLS12_381);
    mapToG1(P, 1);
    mapToG2(Q, 1);

    // simple NIAT
    Fr skI[3], skC;
    G1 pkC;
    G2 pkI[3];
    EQKeyGen(skI, pkI); // is also Issuer KeyGen
    NIATClientKeyGen(skC, pkC);
    // pre-computations
    pairing(eC, pkC, pkI[0]);
    pairing(eI, P, pkI[0]);

    for (int b = 0; b <= 1; b++) {
        niat_psig psig;
        NIATIssue(pkI, &psig, skI, pkC, b);     // issue
    
        niat_token t;
        NIATObtain(&t, skC, pkC, pkI, psig);    // obtain
        
        int b_ = NIATReadBit(skI, pkI, t);      // redeem
        if (!(b == b_)) {
            std::cout << "ERR: bit extraction invalid: expected " << b << " got " << b_ << std::endl;
        }
        else { std::cout << "OK: Bit " << b << " extracted" << std::endl; }
    }

    // NIAT with batched verif.
    int batchSize = 30;

    vector<niat_psig> preSigs(batchSize);
    vector<niat_token> tokens(batchSize);

    // issue a batch of tokens
    for (int i=0; i < batchSize; i++) {
        int b = i % 2;
        NIATIssue(pkI, &preSigs[i], skI, pkC, b);
    }

    // pre-compute for batch verif.
    Fp12::pow(e_pkCm_pkI1m, eC, batchSize);
    Fp12::pow(e_g1n_pkI1n,  eI, batchSize);

    // client verifies a batch
    bool success = NIATClientBatchVerify(pkI, preSigs, batchSize);
    if ( !success ) {
        std::cout << "ERR: NIAT ClientBatchVerify failed" << std::endl;
    }

    // client obtains tokens one by one, but skip eqVerify
    for (int i=0; i < batchSize; i++) {
        NIATObtain(&tokens[i], skC, pkC, pkI, preSigs[i], success);
    }

    // client redeems a batch of tokens

    // issuer verifies a batch
    success = NIATIssuerBatchVerify(pkI, tokens, batchSize);
    if ( ! success ) {
        std::cout << "ERR: NIAT IssuerBatchVerify failed" << std::endl;
    }
    // issuer reads bit one by one, but skip eqVerify
    for (int i=0; i < batchSize; i++) {
        int b_ = NIATReadBit(skI, pkI, tokens[i], success);
        if (b_ != ( i %2) ) {
            std::cout << "ERR: bit extraction invalid: expected " << i % 2 << " got " << b_ << std::endl;
        }
    }
    return 0;
}
