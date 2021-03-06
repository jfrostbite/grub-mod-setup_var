/* This file was automatically imported with 
   import_gcry.py. Please don't modify it */
#include <grub/dl.h>
GRUB_MOD_LICENSE ("GPLv3+");
/* ecc.c  -  Elliptic Curve Cryptography
   Copyright (C) 2007, 2008, 2010 Free Software Foundation, Inc.

   This file is part of Libgcrypt.
  
   Libgcrypt is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.
  
   Libgcrypt is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.
  
   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
   USA.  */

/* This code is originally based on the Patch 0.1.6 for the gnupg
   1.4.x branch as retrieved on 2007-03-21 from
   http://www.calcurco.cat/eccGnuPG/src/gnupg-1.4.6-ecc0.2.0beta1.diff.bz2
   The original authors are:
     Written by
      Sergi Blanch i Torne <d4372211 at alumnes.eup.udl.es>,
      Ramiro Moreno Chiral <ramiro at eup.udl.es>
     Maintainers
      Sergi Blanch i Torne
      Ramiro Moreno Chiral
      Mikael Mylnikov (mmr)
  For use in Libgcrypt the code has been heavily modified and cleaned
  up. In fact there is not much left of the orginally code except for
  some variable names and the text book implementaion of the sign and
  verification algorithms.  The arithmetic functions have entirely
  been rewritten and moved to mpi/ec.c.  */


/* TODO:

  - If we support point compression we need to decide how to compute
    the keygrip - it should not change due to compression.

  - In mpi/ec.c we use mpi_powm for x^2 mod p: Either implement a
    special case in mpi_powm or check whether mpi_mulm is faster.

  - Decide whether we should hide the mpi_point_t definition.

  - Support more than just ECDSA.
*/



#include "g10lib.h"
#include "mpi.h"
#include "cipher.h"


/* Definition of a curve.  */
typedef struct
{
  gcry_mpi_t p;   /* Prime specifying the field GF(p).  */
  gcry_mpi_t a;   /* First coefficient of the Weierstrass equation.  */
  gcry_mpi_t b;   /* Second coefficient of the Weierstrass equation.  */
  mpi_point_t G;  /* Base point (generator).  */
  gcry_mpi_t n;   /* Order of G.  */
} elliptic_curve_t; 


typedef struct
{
  elliptic_curve_t E;
  mpi_point_t Q;  /* Q = [d]G  */
} ECC_public_key;

typedef struct
{
  elliptic_curve_t E;
  mpi_point_t Q;
  gcry_mpi_t d;
} ECC_secret_key;


/* This tables defines aliases for curve names.  */
static const struct
{
  const char *name;  /* Our name.  */
  const char *other; /* Other name. */
} curve_aliases[] = 
  {
    { "NIST P-192", "1.2.840.10045.3.1.1" }, /* X9.62 OID  */
    { "NIST P-192", "prime192v1" },          /* X9.62 name.  */
    { "NIST P-192", "secp192r1"  },          /* SECP name.  */

    { "NIST P-224", "secp224r1" },
    { "NIST P-224", "1.3.132.0.33" },        /* SECP OID.  */

    { "NIST P-256", "1.2.840.10045.3.1.7" }, /* From NIST SP 800-78-1.  */
    { "NIST P-256", "prime256v1" },          
    { "NIST P-256", "secp256r1"  },

    { "NIST P-384", "secp384r1" },
    { "NIST P-384", "1.3.132.0.34" },       

    { "NIST P-521", "secp521r1" },
    { "NIST P-521", "1.3.132.0.35" },

    { "brainpoolP160r1", "1.3.36.3.3.2.8.1.1.1" },
    { "brainpoolP192r1", "1.3.36.3.3.2.8.1.1.3" },
    { "brainpoolP224r1", "1.3.36.3.3.2.8.1.1.5" },
    { "brainpoolP256r1", "1.3.36.3.3.2.8.1.1.7" },
    { "brainpoolP320r1", "1.3.36.3.3.2.8.1.1.9" },
    { "brainpoolP384r1", "1.3.36.3.3.2.8.1.1.11"},
    { "brainpoolP512r1", "1.3.36.3.3.2.8.1.1.13"},

    { NULL, NULL}
  };



/* This static table defines all available curves.  */
static const struct
{
  const char *desc;           /* Description of the curve.  */
  unsigned int nbits;         /* Number of bits.  */
  unsigned int fips:1;        /* True if this is a FIPS140-2 approved curve. */
  const char  *p;             /* Order of the prime field.  */
  const char *a, *b;          /* The coefficients. */
  const char *n;              /* The order of the base point.  */
  const char *g_x, *g_y;      /* Base point.  */
} domain_parms[] =
  {
    {
      "NIST P-192", 192, 1,
      "0xfffffffffffffffffffffffffffffffeffffffffffffffff",
      "0xfffffffffffffffffffffffffffffffefffffffffffffffc",
      "0x64210519e59c80e70fa7e9ab72243049feb8deecc146b9b1",
      "0xffffffffffffffffffffffff99def836146bc9b1b4d22831",

      "0x188da80eb03090f67cbf20eb43a18800f4ff0afd82ff1012",
      "0x07192b95ffc8da78631011ed6b24cdd573f977a11e794811"
    },
    {
      "NIST P-224", 224, 1,
      "0xffffffffffffffffffffffffffffffff000000000000000000000001",
      "0xfffffffffffffffffffffffffffffffefffffffffffffffffffffffe",
      "0xb4050a850c04b3abf54132565044b0b7d7bfd8ba270b39432355ffb4",
      "0xffffffffffffffffffffffffffff16a2e0b8f03e13dd29455c5c2a3d" ,

      "0xb70e0cbd6bb4bf7f321390b94a03c1d356c21122343280d6115c1d21",
      "0xbd376388b5f723fb4c22dfe6cd4375a05a07476444d5819985007e34"
    },
    {
      "NIST P-256", 256, 1,
      "0xffffffff00000001000000000000000000000000ffffffffffffffffffffffff",
      "0xffffffff00000001000000000000000000000000fffffffffffffffffffffffc",
      "0x5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b",
      "0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551",

      "0x6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296",
      "0x4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5"
    },
    {
      "NIST P-384", 384, 1,
      "0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe"
      "ffffffff0000000000000000ffffffff",
      "0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe"
      "ffffffff0000000000000000fffffffc",
      "0xb3312fa7e23ee7e4988e056be3f82d19181d9c6efe8141120314088f5013875a"
      "c656398d8a2ed19d2a85c8edd3ec2aef",
      "0xffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf"
      "581a0db248b0a77aecec196accc52973",

      "0xaa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a38"
      "5502f25dbf55296c3a545e3872760ab7",
      "0x3617de4a96262c6f5d9e98bf9292dc29f8f41dbd289a147ce9da3113b5f0b8c0"
      "0a60b1ce1d7e819d7a431d7c90ea0e5f"
    },
    {
      "NIST P-521", 521, 1,
      "0x01ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
      "0x01ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffc",
      "0x051953eb9618e1c9a1f929a21a0b68540eea2da725b99b315f3b8b489918ef10"
      "9e156193951ec7e937b1652c0bd3bb1bf073573df883d2c34f1ef451fd46b503f00",
      "0x1fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffa51868783bf2f966b7fcc0148f709a5d03bb5c9b8899c47aebb6fb71e91386409",

      "0xc6858e06b70404e9cd9e3ecb662395b4429c648139053fb521f828af606b4d3d"
      "baa14b5e77efe75928fe1dc127a2ffa8de3348b3c1856a429bf97e7e31c2e5bd66",
      "0x11839296a789a3bc0045c8a5fb42c7d1bd998f54449579b446817afbd17273e6"
      "62c97ee72995ef42640c550b9013fad0761353c7086a272c24088be94769fd16650"
    },

    { "brainpoolP160r1", 160, 0, 
      "0xe95e4a5f737059dc60dfc7ad95b3d8139515620f",
      "0x340e7be2a280eb74e2be61bada745d97e8f7c300",
      "0x1e589a8595423412134faa2dbdec95c8d8675e58",
      "0xe95e4a5f737059dc60df5991d45029409e60fc09",
      "0xbed5af16ea3f6a4f62938c4631eb5af7bdbcdbc3",
      "0x1667cb477a1a8ec338f94741669c976316da6321"
    },

    { "brainpoolP192r1", 192, 0, 
      "0xc302f41d932a36cda7a3463093d18db78fce476de1a86297",
      "0x6a91174076b1e0e19c39c031fe8685c1cae040e5c69a28ef",
      "0x469a28ef7c28cca3dc721d044f4496bcca7ef4146fbf25c9",
      "0xc302f41d932a36cda7a3462f9e9e916b5be8f1029ac4acc1",
      "0xc0a0647eaab6a48753b033c56cb0f0900a2f5c4853375fd6",
      "0x14b690866abd5bb88b5f4828c1490002e6773fa2fa299b8f"
    },

    { "brainpoolP224r1", 224, 0,
      "0xd7c134aa264366862a18302575d1d787b09f075797da89f57ec8c0ff",
      "0x68a5e62ca9ce6c1c299803a6c1530b514e182ad8b0042a59cad29f43",
      "0x2580f63ccfe44138870713b1a92369e33e2135d266dbb372386c400b",
      "0xd7c134aa264366862a18302575d0fb98d116bc4b6ddebca3a5a7939f",
      "0x0d9029ad2c7e5cf4340823b2a87dc68c9e4ce3174c1e6efdee12c07d",
      "0x58aa56f772c0726f24c6b89e4ecdac24354b9e99caa3f6d3761402cd"
    },

    { "brainpoolP256r1", 256, 0,
      "0xa9fb57dba1eea9bc3e660a909d838d726e3bf623d52620282013481d1f6e5377",
      "0x7d5a0975fc2c3057eef67530417affe7fb8055c126dc5c6ce94a4b44f330b5d9",
      "0x26dc5c6ce94a4b44f330b5d9bbd77cbf958416295cf7e1ce6bccdc18ff8c07b6",
      "0xa9fb57dba1eea9bc3e660a909d838d718c397aa3b561a6f7901e0e82974856a7",
      "0x8bd2aeb9cb7e57cb2c4b482ffc81b7afb9de27e1e3bd23c23a4453bd9ace3262",
      "0x547ef835c3dac4fd97f8461a14611dc9c27745132ded8e545c1d54c72f046997"
    },

    { "brainpoolP320r1", 320, 0,
      "0xd35e472036bc4fb7e13c785ed201e065f98fcfa6f6f40def4f92b9ec7893ec28"
      "fcd412b1f1b32e27",
      "0x3ee30b568fbab0f883ccebd46d3f3bb8a2a73513f5eb79da66190eb085ffa9f4"
      "92f375a97d860eb4",
      "0x520883949dfdbc42d3ad198640688a6fe13f41349554b49acc31dccd88453981"
      "6f5eb4ac8fb1f1a6",
      "0xd35e472036bc4fb7e13c785ed201e065f98fcfa5b68f12a32d482ec7ee8658e9"
      "8691555b44c59311",
      "0x43bd7e9afb53d8b85289bcc48ee5bfe6f20137d10a087eb6e7871e2a10a599c7"
      "10af8d0d39e20611",
      "0x14fdd05545ec1cc8ab4093247f77275e0743ffed117182eaa9c77877aaac6ac7"
      "d35245d1692e8ee1"
    },

    { "brainpoolP384r1", 384, 0,
      "0x8cb91e82a3386d280f5d6f7e50e641df152f7109ed5456b412b1da197fb71123"
      "acd3a729901d1a71874700133107ec53",
      "0x7bc382c63d8c150c3c72080ace05afa0c2bea28e4fb22787139165efba91f90f"
      "8aa5814a503ad4eb04a8c7dd22ce2826",
      "0x04a8c7dd22ce28268b39b55416f0447c2fb77de107dcd2a62e880ea53eeb62d5"
      "7cb4390295dbc9943ab78696fa504c11",
      "0x8cb91e82a3386d280f5d6f7e50e641df152f7109ed5456b31f166e6cac0425a7"
      "cf3ab6af6b7fc3103b883202e9046565",
      "0x1d1c64f068cf45ffa2a63a81b7c13f6b8847a3e77ef14fe3db7fcafe0cbd10e8"
      "e826e03436d646aaef87b2e247d4af1e",
      "0x8abe1d7520f9c2a45cb1eb8e95cfd55262b70b29feec5864e19c054ff9912928"
      "0e4646217791811142820341263c5315"
    },

    { "brainpoolP512r1", 512, 0,
      "0xaadd9db8dbe9c48b3fd4e6ae33c9fc07cb308db3b3c9d20ed6639cca70330871"
      "7d4d9b009bc66842aecda12ae6a380e62881ff2f2d82c68528aa6056583a48f3",
      "0x7830a3318b603b89e2327145ac234cc594cbdd8d3df91610a83441caea9863bc"
      "2ded5d5aa8253aa10a2ef1c98b9ac8b57f1117a72bf2c7b9e7c1ac4d77fc94ca",
      "0x3df91610a83441caea9863bc2ded5d5aa8253aa10a2ef1c98b9ac8b57f1117a7"
      "2bf2c7b9e7c1ac4d77fc94cadc083e67984050b75ebae5dd2809bd638016f723",
      "0xaadd9db8dbe9c48b3fd4e6ae33c9fc07cb308db3b3c9d20ed6639cca70330870"
      "553e5c414ca92619418661197fac10471db1d381085ddaddb58796829ca90069",
      "0x81aee4bdd82ed9645a21322e9c4c6a9385ed9f70b5d916c1b43b62eef4d0098e"
      "ff3b1f78e2d0d48d50d1687b93b97d5f7c6d5047406a5e688b352209bcb9f822",
      "0x7dde385d566332ecc0eabfa9cf7822fdf209f70024a57b1aa000c55b881f8111"
      "b2dcde494a5f485e5bca4bd88a2763aed1ca2b2fa8f0540678cd1e0f3ad80892"
    },

    { NULL, 0, 0, NULL, NULL, NULL, NULL }
  };


/* Registered progress function and its callback value. */


#define point_init(a)  _gcry_mpi_ec_point_init ((a))
#define point_free(a)  _gcry_mpi_ec_point_free ((a))



/* Local prototypes. */
static int check_secret_key (ECC_secret_key * sk);
static gpg_err_code_t verify (gcry_mpi_t input, ECC_public_key *pkey,
                              gcry_mpi_t r, gcry_mpi_t s);


static gcry_mpi_t gen_y_2 (gcry_mpi_t x, elliptic_curve_t * base);





/* static void */
/* progress (int c) */
/* { */
/*   if (progress_cb) */
/*     progress_cb (progress_cb_data, "pk_ecc", c, 0, 0); */
/* } */




/* Set the value from S into D.  */


/*
 * Release a curve object.
 */


/*
 * Return a copy of a curve object.
 */



/* Helper to scan a hex string. */





/****************
 * Solve the right side of the equation that defines a curve.
 */
static gcry_mpi_t
gen_y_2 (gcry_mpi_t x, elliptic_curve_t *base)
{
  gcry_mpi_t three, x_3, axb, y;

  three = mpi_alloc_set_ui (3);
  x_3 = mpi_new (0);
  axb = mpi_new (0);
  y   = mpi_new (0);

  mpi_powm (x_3, x, three, base->p);  
  mpi_mulm (axb, base->a, x, base->p); 
  mpi_addm (axb, axb, base->b, base->p);     
  mpi_addm (y, x_3, axb, base->p);    

  mpi_free (x_3);
  mpi_free (axb);
  mpi_free (three);
  return y; /* The quadratic value of the coordinate if it exist. */
}





/* Generate a random secret scalar k with an order of p

   At the beginning this was identical to the code is in elgamal.c.
   Later imporved by mmr.   Further simplified by wk.  */

/****************
 * Generate the crypto system setup.
 * As of now the fix NIST recommended values are used.
 * The subgroup generator point is in another function: gen_big_point.
 */


/*
 * First obtain the setup.  Over the finite field randomize an scalar
 * secret value, and calculate the public point.
 */


/****************
 * To verify correct skey it use a random information.
 * First, encrypt and decrypt this dummy value,
 * test if the information is recuperated.
 * Second, test with the sign and verify functions.
 */

/****************
 * To check the validity of the value, recalculate the correspondence
 * between the public value and the secret one.
 */
static int
check_secret_key (ECC_secret_key * sk)
{
  mpi_point_t Q;
  gcry_mpi_t y_2, y2 = mpi_alloc (0);
  mpi_ec_t ctx;

  /* ?primarity test of 'p' */
  /*  (...) //!! */
  /* G in E(F_p) */
  y_2 = gen_y_2 (sk->E.G.x, &sk->E);   /*  y^2=x^3+a*x+b */
  mpi_mulm (y2, sk->E.G.y, sk->E.G.y, sk->E.p);      /*  y^2=y*y */
  if (mpi_cmp (y_2, y2))
    {
      if (DBG_CIPHER)
        log_debug ("Bad check: Point 'G' does not belong to curve 'E'!\n");
      return (1);
    }
  /* G != PaI */
  if (!mpi_cmp_ui (sk->E.G.z, 0))
    {
      if (DBG_CIPHER)
        log_debug ("Bad check: 'G' cannot be Point at Infinity!\n");
      return (1);
    }

  point_init (&Q);
  ctx = _gcry_mpi_ec_init (sk->E.p, sk->E.a);
  _gcry_mpi_ec_mul_point (&Q, sk->E.n, &sk->E.G, ctx);
  if (mpi_cmp_ui (Q.z, 0))
    {
      if (DBG_CIPHER)
        log_debug ("check_secret_key: E is not a curve of order n\n");
      point_free (&Q);
      _gcry_mpi_ec_free (ctx);
      return 1;
    }
  /* pubkey cannot be PaI */
  if (!mpi_cmp_ui (sk->Q.z, 0))
    {
      if (DBG_CIPHER)
        log_debug ("Bad check: Q can not be a Point at Infinity!\n");
      _gcry_mpi_ec_free (ctx);
      return (1);
    }
  /* pubkey = [d]G over E */
  _gcry_mpi_ec_mul_point (&Q, sk->d, &sk->E.G, ctx);
  if ((Q.x == sk->Q.x) && (Q.y == sk->Q.y) && (Q.z == sk->Q.z))
    {
      if (DBG_CIPHER)
        log_debug
          ("Bad check: There is NO correspondence between 'd' and 'Q'!\n");
      _gcry_mpi_ec_free (ctx);
      return (1);
    }
  _gcry_mpi_ec_free (ctx);
  point_free (&Q);
  return 0;
}


/*
 * Return the signature struct (r,s) from the message hash.  The caller
 * must have allocated R and S.
 */

/*
 * Check if R and S verifies INPUT.
 */
static gpg_err_code_t
verify (gcry_mpi_t input, ECC_public_key *pkey, gcry_mpi_t r, gcry_mpi_t s)
{
  gpg_err_code_t err = 0;
  gcry_mpi_t h, h1, h2, x, y;
  mpi_point_t Q, Q1, Q2;
  mpi_ec_t ctx;

  if( !(mpi_cmp_ui (r, 0) > 0 && mpi_cmp (r, pkey->E.n) < 0) )
    return GPG_ERR_BAD_SIGNATURE; /* Assertion	0 < r < n  failed.  */
  if( !(mpi_cmp_ui (s, 0) > 0 && mpi_cmp (s, pkey->E.n) < 0) )
    return GPG_ERR_BAD_SIGNATURE; /* Assertion	0 < s < n  failed.  */

  h  = mpi_alloc (0);
  h1 = mpi_alloc (0);
  h2 = mpi_alloc (0);
  x = mpi_alloc (0);
  y = mpi_alloc (0);
  point_init (&Q);
  point_init (&Q1);
  point_init (&Q2);

  ctx = _gcry_mpi_ec_init (pkey->E.p, pkey->E.a);

  /* h  = s^(-1) (mod n) */
  mpi_invm (h, s, pkey->E.n);
/*   log_mpidump ("   h", h); */
  /* h1 = hash * s^(-1) (mod n) */
  mpi_mulm (h1, input, h, pkey->E.n);
/*   log_mpidump ("  h1", h1); */
  /* Q1 = [ hash * s^(-1) ]G  */
  _gcry_mpi_ec_mul_point (&Q1, h1, &pkey->E.G, ctx);
/*   log_mpidump ("Q1.x", Q1.x); */
/*   log_mpidump ("Q1.y", Q1.y); */
/*   log_mpidump ("Q1.z", Q1.z); */
  /* h2 = r * s^(-1) (mod n) */
  mpi_mulm (h2, r, h, pkey->E.n);
/*   log_mpidump ("  h2", h2); */
  /* Q2 = [ r * s^(-1) ]Q */
  _gcry_mpi_ec_mul_point (&Q2, h2, &pkey->Q, ctx);
/*   log_mpidump ("Q2.x", Q2.x); */
/*   log_mpidump ("Q2.y", Q2.y); */
/*   log_mpidump ("Q2.z", Q2.z); */
  /* Q  = ([hash * s^(-1)]G) + ([r * s^(-1)]Q) */
  _gcry_mpi_ec_add_points (&Q, &Q1, &Q2, ctx);
/*   log_mpidump (" Q.x", Q.x); */
/*   log_mpidump (" Q.y", Q.y); */
/*   log_mpidump (" Q.z", Q.z); */

  if (!mpi_cmp_ui (Q.z, 0))
    {
      if (DBG_CIPHER)
          log_debug ("ecc verify: Rejected\n");
      err = GPG_ERR_BAD_SIGNATURE;
      goto leave;
    }
  if (_gcry_mpi_ec_get_affine (x, y, &Q, ctx))
    {
      if (DBG_CIPHER)
        log_debug ("ecc verify: Failed to get affine coordinates\n");
      err = GPG_ERR_BAD_SIGNATURE;
      goto leave;
    }
  mpi_mod (x, x, pkey->E.n); /* x = x mod E_n */
  if (mpi_cmp (x, r))   /* x != r */
    {
      if (DBG_CIPHER)
        {
          log_mpidump ("   x", x);
          log_mpidump ("   y", y);
          log_mpidump ("   r", r);
          log_mpidump ("   s", s);
          log_debug ("ecc verify: Not verified\n");
        }
      err = GPG_ERR_BAD_SIGNATURE;
      goto leave;
    }
  if (DBG_CIPHER)
    log_debug ("ecc verify: Accepted\n");

 leave:
  _gcry_mpi_ec_free (ctx);
  point_free (&Q2);
  point_free (&Q1);
  point_free (&Q);
  mpi_free (y);
  mpi_free (x);
  mpi_free (h2);
  mpi_free (h1);
  mpi_free (h);
  return err;
}



/*********************************************
 **************  interface  ******************
 *********************************************/

/* RESULT must have been initialized and is set on success to the
   point given by VALUE.  */
static gcry_error_t
os2ec (mpi_point_t *result, gcry_mpi_t value)
{
  gcry_error_t err;
  size_t n;
  unsigned char *buf;
  gcry_mpi_t x, y;

  n = (mpi_get_nbits (value)+7)/8;
  buf = gcry_xmalloc (n);
  err = gcry_mpi_print (GCRYMPI_FMT_USG, buf, n, &n, value);
  if (err)
    {
      gcry_free (buf);
      return err;
    }
  if (n < 1) 
    {
      gcry_free (buf);
      return GPG_ERR_INV_OBJ;
    }
  if (*buf != 4)
    {
      gcry_free (buf);
      return GPG_ERR_NOT_IMPLEMENTED; /* No support for point compression.  */
    }
  if ( ((n-1)%2) ) 
    {
      gcry_free (buf);
      return GPG_ERR_INV_OBJ;
    }
  n = (n-1)/2;
  err = gcry_mpi_scan (&x, GCRYMPI_FMT_USG, buf+1, n, NULL);
  if (err)
    {
      gcry_free (buf);
      return err;
    }
  err = gcry_mpi_scan (&y, GCRYMPI_FMT_USG, buf+1+n, n, NULL);
  gcry_free (buf);
  if (err)
    {
      mpi_free (x);
      return err;
    }

  mpi_set (result->x, x);
  mpi_set (result->y, y);
  mpi_set_ui (result->z, 1);

  mpi_free (x);
  mpi_free (y);
  
  return 0;
}


/* Extended version of ecc_generate.  */
#define ecc_generate 0

#define ecc_generate 0

/* Return the parameters of the curve NAME.  */


static gcry_err_code_t
ecc_check_secret_key (int algo, gcry_mpi_t *skey)
{
  gpg_err_code_t err;
  ECC_secret_key sk;

  (void)algo;

  if (!skey[0] || !skey[1] || !skey[2] || !skey[3] || !skey[4] || !skey[5]
      || !skey[6] || !skey[7] || !skey[8] || !skey[9] || !skey[10])
    return GPG_ERR_BAD_MPI;

  sk.E.p = skey[0];
  sk.E.a = skey[1];
  sk.E.b = skey[2];
  point_init (&sk.E.G);
  err = os2ec (&sk.E.G, skey[3]);
  if (err)
    {
      point_free (&sk.E.G);
      return err;
    }
  sk.E.n = skey[4];
  point_init (&sk.Q);
  err = os2ec (&sk.Q, skey[5]);
  if (err)
    {
      point_free (&sk.E.G);
      point_free (&sk.Q);
      return err;
    }

  sk.d = skey[6];

  if (check_secret_key (&sk))
    {
      point_free (&sk.E.G);
      point_free (&sk.Q);
      return GPG_ERR_BAD_SECKEY;
    }
  point_free (&sk.E.G);
  point_free (&sk.Q);
  return 0;
}


#define ecc_sign 0
static gcry_err_code_t
ecc_verify (int algo, gcry_mpi_t hash, gcry_mpi_t *data, gcry_mpi_t *pkey,
            int (*cmp)(void *, gcry_mpi_t), void *opaquev)
{
  gpg_err_code_t err;
  ECC_public_key pk;

  (void)algo;
  (void)cmp;
  (void)opaquev;

  if (!data[0] || !data[1] || !hash || !pkey[0] || !pkey[1] || !pkey[2]
      || !pkey[3] || !pkey[4] || !pkey[5] )
    return GPG_ERR_BAD_MPI;

  pk.E.p = pkey[0];
  pk.E.a = pkey[1];
  pk.E.b = pkey[2];
  point_init (&pk.E.G);
  err = os2ec (&pk.E.G, pkey[3]);
  if (err)
    {
      point_free (&pk.E.G);
      return err;
    }
  pk.E.n = pkey[4];
  point_init (&pk.Q);
  err = os2ec (&pk.Q, pkey[5]);
  if (err)
    {
      point_free (&pk.E.G);
      point_free (&pk.Q);
      return err;
    }

  err = verify (hash, &pk, data[0], data[1]);

  point_free (&pk.E.G);
  point_free (&pk.Q);
  return err;
}



static unsigned int
ecc_get_nbits (int algo, gcry_mpi_t *pkey)
{
  (void)algo;

  return mpi_get_nbits (pkey[0]);
}



/* See rsa.c for a description of this function.  */





/* 
     Self-test section.
 */




/* Run a full self-test for ALGO and return 0 on success.  */




static const char *ecdsa_names[] =
  {
    "ecdsa",
    "ecc",
    NULL,
  };

gcry_pk_spec_t _gcry_pubkey_spec_ecdsa =
  {
    "ECDSA", ecdsa_names,
    "pabgnq", "pabgnqd", "", "rs", "pabgnq",
    GCRY_PK_USAGE_SIGN,
    ecc_generate,
    ecc_check_secret_key,
    NULL,
    NULL,
    ecc_sign,
    ecc_verify,
    ecc_get_nbits
    ,
#ifdef GRUB_UTIL
    .modname = "gcry_ecc",
#endif
  };




GRUB_MOD_INIT(gcry_ecc)
{
  grub_crypto_pk_ecdsa = &_gcry_pubkey_spec_ecdsa;
}

GRUB_MOD_FINI(gcry_ecc)
{
  grub_crypto_pk_ecdsa = 0;
}
