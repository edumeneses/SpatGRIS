/*
 * Functions for 3D VBAP processing based on work by Ville Pulkki.
 * (c) Ville Pulkki - 2.2.1999 Helsinki University of Technology.
 * Updated by belangeo, 2017.
 */

#include "vbap.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "LogicStrucs.hpp"
#include "StrongTypes.hpp"
#include "narrow.hpp"

struct TripletData {
    int tripletSpeakerNumber[3];   /* Triplet speaker numbers */
    float tripletInverseMatrix[9]; /* Triplet inverse matrix */
};

using TripletList = std::vector<TripletData>;

/* Selects a vector base of a virtual source.
 * Calculates gain factors in that base. */
static void compute_gains(juce::Array<SpeakerSet> & sets,
                          SpeakersSpatGains & gains,
                          int const numSpeakers,
                          CartesianVector const cart_dir,
                          int const dim) noexcept
{
    float vec[3];
    /* Direction of the virtual source in cartesian coordinates. */
    vec[0] = cart_dir.x;
    vec[1] = cart_dir.y;
    vec[2] = cart_dir.z;

    for (auto & set : sets) {
        set.setGains[0] = 0.0;
        set.setGains[1] = 0.0;
        set.setGains[2] = 0.0;
        set.smallestWt = 1000.0;
        set.neg_g_am = 0;
    }

    for (auto & set : sets) {
        for (int j = 0; j < dim; j++) {
            for (int k = 0; k < dim; k++) {
                set.setGains[j] += vec[k] * set.invMx[((dim * j) + k)];
            }
            if (set.smallestWt > set.setGains[j])
                set.smallestWt = set.setGains[j];
            if (set.setGains[j] < -0.05)
                set.neg_g_am++;
        }
    }

    int j = 0;
    float tmp = sets[0].smallestWt;
    int tmp2 = sets[0].neg_g_am;
    for (int i = 1; i < sets.size(); i++) {
        if (sets[i].neg_g_am < tmp2) {
            tmp = sets[i].smallestWt;
            tmp2 = sets[i].neg_g_am;
            j = i;
        } else if (sets[i].neg_g_am == tmp2) {
            if (sets[i].smallestWt > tmp) {
                tmp = sets[i].smallestWt;
                tmp2 = sets[i].neg_g_am;
                j = i;
            }
        }
    }

    if (sets[j].setGains[0] <= 0.0 && sets[j].setGains[1] <= 0.0 && sets[j].setGains[2] <= 0.0) {
        sets[j].setGains[0] = 1.0;
        sets[j].setGains[1] = 1.0;
        sets[j].setGains[2] = 1.0;
    }

    auto * rawGains{ gains.data() };
    rawGains[sets[j].speakerNos[0] - 1] = sets[j].setGains[0];
    rawGains[sets[j].speakerNos[1] - 1] = sets[j].setGains[1];
    if (dim == 3)
        rawGains[sets[j].speakerNos[2] - 1] = sets[j].setGains[2];

    for (int i = 0; i < numSpeakers; i++) {
        if (rawGains[i] < 0.0)
            rawGains[i] = 0.0;
    }
}

/* Returns 1 if there is loudspeaker(s) inside given ls triplet. */
static bool any_speaker_inside_triplet(int const a,
                                       int const b,
                                       int const c,
                                       std::array<LoudSpeaker, MAX_OUTPUTS> const & speakers,
                                       int const numSpeakers) noexcept
{
    InverseMatrix inverseMatrix;

    auto const * const lp1 = &(speakers[a].coords);
    auto const * const lp2 = &(speakers[b].coords);
    auto const * const lp3 = &(speakers[c].coords);

    /* Matrix inversion. */
    auto const invdet
        = 1.0f
          / (lp1->x * ((lp2->y * lp3->z) - (lp2->z * lp3->y)) - lp1->y * ((lp2->x * lp3->z) - (lp2->z * lp3->x))
             + lp1->z * ((lp2->x * lp3->y) - (lp2->y * lp3->x)));

    inverseMatrix[0] = ((lp2->y * lp3->z) - (lp2->z * lp3->y)) * invdet;
    inverseMatrix[3] = ((lp1->y * lp3->z) - (lp1->z * lp3->y)) * -invdet;
    inverseMatrix[6] = ((lp1->y * lp2->z) - (lp1->z * lp2->y)) * invdet;
    inverseMatrix[1] = ((lp2->x * lp3->z) - (lp2->z * lp3->x)) * -invdet;
    inverseMatrix[4] = ((lp1->x * lp3->z) - (lp1->z * lp3->x)) * invdet;
    inverseMatrix[7] = ((lp1->x * lp2->z) - (lp1->z * lp2->x)) * -invdet;
    inverseMatrix[2] = ((lp2->x * lp3->y) - (lp2->y * lp3->x)) * invdet;
    inverseMatrix[5] = ((lp1->x * lp3->y) - (lp1->y * lp3->x)) * -invdet;
    inverseMatrix[8] = ((lp1->x * lp2->y) - (lp1->y * lp2->x)) * invdet;

    for (int i{}; i < numSpeakers; ++i) {
        if (i != a && i != b && i != c) {
            auto this_inside{ true };
            for (int j{}; j < 3; ++j) {
                auto tmp = speakers[i].coords.x * inverseMatrix[0 + j * 3];
                tmp += speakers[i].coords.y * inverseMatrix[1 + j * 3];
                tmp += speakers[i].coords.z * inverseMatrix[2 + j * 3];
                if (tmp < -0.001f) {
                    this_inside = false;
                    break;
                }
            }
            if (this_inside) {
                return true;
            }
        }
    }
    return false;
}

/* Checks if two lines intersect on 3D sphere see theory in paper
 * Pulkki, V. Lokki, T. "Creating Auditory Displays with Multiple
 * Loudspeakers Using VBAP: A Case Study with DIVA Project" in
 * International Conference on Auditory Displays -98.
 * E-mail Ville.Pulkki@hut.fi if you want to have that paper. */
static int lines_intersect(int const i,
                           int const j,
                           int const k,
                           int const l,
                           std::array<LoudSpeaker, MAX_OUTPUTS> const & speakers) noexcept
{
    auto const v1{ speakers[i].coords.crossProduct(speakers[j].coords) };
    auto const v2{ speakers[k].coords.crossProduct(speakers[l].coords) };
    auto const v3{ v1.crossProduct(v2) };
    auto const negV3{ -v3 };

    auto const dist_ij = speakers[i].coords.angleWith(speakers[j].coords);
    auto const dist_kl = speakers[k].coords.angleWith(speakers[l].coords);
    auto const dist_iv3 = speakers[i].coords.angleWith(v3);
    auto const dist_jv3 = v3.angleWith(speakers[j].coords);
    auto const dist_inv3 = speakers[i].coords.angleWith(negV3);
    auto const dist_jnv3 = negV3.angleWith(speakers[j].coords);
    auto const dist_kv3 = speakers[k].coords.angleWith(v3);
    auto const dist_lv3 = v3.angleWith(speakers[l].coords);
    auto const dist_knv3 = speakers[k].coords.angleWith(negV3);
    auto const dist_lnv3 = negV3.angleWith(speakers[l].coords);

    /*One of loudspeakers is close to crossing point, don't do anything.*/
    if (std::abs(dist_iv3) <= 0.01 || std::abs(dist_jv3) <= 0.01 || std::abs(dist_kv3) <= 0.01
        || std::abs(dist_lv3) <= 0.01 || std::abs(dist_inv3) <= 0.01 || std::abs(dist_jnv3) <= 0.01
        || std::abs(dist_knv3) <= 0.01 || std::abs(dist_lnv3) <= 0.01) {
        return (0);
    }

    if (((std::abs(dist_ij - (dist_iv3 + dist_jv3)) <= 0.01) && (std::abs(dist_kl - (dist_kv3 + dist_lv3)) <= 0.01))
        || ((std::abs(dist_ij - (dist_inv3 + dist_jnv3)) <= 0.01)
            && (std::abs(dist_kl - (dist_knv3 + dist_lnv3)) <= 0.01))) {
        return (1);
    }
    return (0);
}

static void spreadit_azi_ele(SourceData const & source, SpeakersSpatGains & gains, VbapData & data) noexcept
{
    int ind;
    static constexpr auto num = 4;
    degrees_t newAzimuth;
    degrees_t newElevation;
    int const cnt = data.numSpeakers;
    float sum = 0.0f;
    SpeakersSpatGains tmp_gains{};

    auto const sp_azi = std::clamp(source.azimuthSpan, 0.0f, 1.0f);
    auto const sp_ele = std::clamp(source.zenithSpan, 0.0f, 1.0f);

    // If both sp_azi and sp_ele are active, we want to put a virtual source at
    // (azi, ele +/- eledev) and (azi +/- azidev, ele) locations.
    auto const kNum{ sp_azi > 0.0 && sp_ele > 0.0 ? 8 : 4 };

    auto * rawGains{ gains.data() };

    for (int i{}; i < num; ++i) {
        auto const iFloat{ narrow<float>(i) };
        auto const compensation = std::pow(10.0f, (iFloat + 1.0f) * -3.0f * 0.05f);
        auto const azimuthDev = degrees_t{ 45.0f } * (iFloat + 1.0f) * sp_azi;
        auto const elevationDev = degrees_t{ 22.5f } * (iFloat + 1.0f) * sp_ele;
        for (int k{}; k < kNum; ++k) {
            switch (k) {
            case 0:
                newAzimuth = data.angularDirection.azimuth + azimuthDev;
                newElevation = data.angularDirection.elevation + elevationDev;
                break;
            case 1:
                newAzimuth = data.angularDirection.azimuth - azimuthDev;
                newElevation = data.angularDirection.elevation - elevationDev;
                break;
            case 2:
                newAzimuth = data.angularDirection.azimuth + azimuthDev;
                newElevation = data.angularDirection.elevation - elevationDev;
                break;
            case 3:
                newAzimuth = data.angularDirection.azimuth - azimuthDev;
                newElevation = data.angularDirection.elevation + elevationDev;
                break;
            case 4:
                newAzimuth = data.angularDirection.azimuth;
                newElevation = data.angularDirection.elevation + elevationDev;
                break;
            case 5:
                newAzimuth = data.angularDirection.azimuth;
                newElevation = data.angularDirection.elevation - elevationDev;
                break;
            case 6:
                newAzimuth = data.angularDirection.azimuth + azimuthDev;
                newElevation = data.angularDirection.elevation;
                break;
            case 7:
                newAzimuth = data.angularDirection.azimuth - azimuthDev;
                newElevation = data.angularDirection.elevation;
                break;
            default:
                jassertfalse;
            }

            newElevation = std::clamp(newElevation, degrees_t{}, HALF_PI.toDegrees());
            newAzimuth = newAzimuth.centered();
            PolarVector const spreadAngle{ newAzimuth, newElevation, 1.0f };
            auto const spreadCartesian{ spreadAngle.toCartesian() };
            compute_gains(data.speakerSets, tmp_gains, data.numSpeakers, spreadCartesian, data.dimension);
            std::transform(gains.cbegin(),
                           gains.cend(),
                           tmp_gains.cbegin(),
                           gains.begin(),
                           [compensation](float const a, float const b) { return a + b * compensation; });
            // for (int j{}; j < cnt; ++j) {
            //    rawGains[j] += rawTmpGains[j] * comp;
            //}
        }
    }

    if (sp_azi > 0.8f && sp_ele > 0.8f) {
        auto const compensation = (sp_azi - 0.8f) / 0.2f * (sp_ele - 0.8f) / 0.2f * 10.0f;
        for (int i{}; i < data.numOutputPatches; ++i) {
            rawGains[data.outputPatches[i].get() - 1] += compensation;
        }
    }

    for (int i{}; i < data.numOutputPatches; ++i) {
        ind = data.outputPatches[i].get() - 1;
        sum += (rawGains[ind] * rawGains[ind]);
    }
    sum = std::sqrt(sum);
    for (int i{}; i < data.numOutputPatches; ++i) {
        ind = data.outputPatches[i].get() - 1;
        rawGains[ind] /= sum;
    }
}

static void spreadit_azi(SourceData const & source, SpeakersSpatGains & gains, VbapData & data)
{
    int i;
    static constexpr auto num = 4;
    degrees_t newazi{};
    PolarVector spreadang;
    int cnt = data.numSpeakers;
    SpeakersSpatGains tmp_gains{};
    auto * rawGains{ gains.data() };
    float sum = 0.0;

    auto const aznimuthSpread{ std::clamp(source.azimuthSpan, 0.0f, 1.0f) };

    for (i = 0; i < num; i++) {
        float comp = std::pow(10.0f, (i + 1) * -3.0f * 0.05f);
        degrees_t const azidev{ (i + 1) * aznimuthSpread * 45.0f };
        for (int k = 0; k < 2; k++) {
            if (k == 0) {
                newazi = data.angularDirection.azimuth + azidev;
            } else if (k == 1) {
                newazi = data.angularDirection.azimuth - azidev;
            }
            newazi = newazi.centered();
            spreadang.azimuth = newazi;
            spreadang.elevation = degrees_t{};
            spreadang.length = 1.0;
            CartesianVector spreadcart = spreadang.toCartesian();
            compute_gains(data.speakerSets, tmp_gains, data.numSpeakers, spreadcart, data.dimension);
            auto const * rawTmpGains{ tmp_gains.data() };
            for (int j = 0; j < cnt; j++) {
                rawGains[j] += (rawTmpGains[j] * comp);
            }
        }
    }

    for (i = 0; i < cnt; i++) {
        sum += (rawGains[i] * rawGains[i]);
    }
    sum = std::sqrt(sum);
    for (i = 0; i < cnt; i++) {
        rawGains[i] /= sum;
    }
}

/*
 * No external use.
 */
static void sort_2D_lss(std::array<LoudSpeaker, MAX_OUTPUTS> & speakers,
                        std::array<int, MAX_OUTPUTS> & sortedSpeakers,
                        int const numSpeakers)
{
    // TODO: this whole function needs to be rewritten

    int i, j, index = 0;
    float tmp, tmp_azi;

    /* Transforming angles between -180 and 180. */
    for (i = 0; i < numSpeakers; i++) {
        speakers[i].coords = speakers[i].angles.toCartesian();
        speakers[i].angles.azimuth = degrees_t{ std::acos(speakers[i].coords.x) }; // TODO: weird
        if (std::abs(speakers[i].coords.y) <= 0.001)
            tmp = 1.0;
        else
            tmp = speakers[i].coords.y / std::abs(speakers[i].coords.y);
        speakers[i].angles.azimuth *= tmp;
    }
    for (i = 0; i < numSpeakers; i++) {
        tmp = 2000;
        for (j = 0; j < numSpeakers; j++) {
            if (speakers[j].angles.azimuth.get() <= tmp) {
                tmp = speakers[j].angles.azimuth.get();
                index = j;
            }
        }
        sortedSpeakers[i] = index;
        tmp_azi = speakers[index].angles.azimuth.get();
        speakers[index].angles.azimuth = degrees_t{ tmp_azi + 4000.0f };
    }
    for (i = 0; i < numSpeakers; i++) {
        tmp_azi = speakers[i].angles.azimuth.get();
        speakers[i].angles.azimuth = degrees_t{ tmp_azi - 4000.0f };
    }
}

/*
 * No external use.
 */
static int calc_2D_inv_tmatrix(radians_t const azi1, radians_t const azi2, float inv_mat[4])
{
    auto const x1 = fast::cos(azi1.get());
    auto const x2 = fast::sin(azi1.get());
    auto const x3 = fast::cos(azi2.get());
    auto const x4 = fast::sin(azi2.get());
    auto const det = (x1 * x4) - (x3 * x2);
    if (std::abs(det) <= 0.001) {
        inv_mat[0] = 0.0;
        inv_mat[1] = 0.0;
        inv_mat[2] = 0.0;
        inv_mat[3] = 0.0;
        return 0;
    }
    inv_mat[0] = x4 / det;
    inv_mat[1] = -x3 / det;
    inv_mat[2] = -x2 / det;
    inv_mat[3] = x1 / det;
    return 1;
}

/* Selects the loudspeaker pairs, calculates the inversion
 * matrices and stores the data to a global array.
 */
static void
    choose_ls_tuplets(std::array<LoudSpeaker, MAX_OUTPUTS> & speakers, TripletList & triplets, int const numSpeakers)
{
    std::array<int, MAX_OUTPUTS> exist{};
    std::array<int, MAX_OUTPUTS> sortedSpeakers;
    /* Sort loudspeakers according their azimuth angle. */
    sort_2D_lss(speakers, sortedSpeakers, numSpeakers);

    /* Adjacent loudspeakers are the loudspeaker pairs to be used. */
    int amount = 0;
    float inv_mat[MAX_OUTPUTS][4];
    for (int i = 0; i < (numSpeakers - 1); i++) {
        if (speakers[sortedSpeakers[i + 1]].angles.azimuth - speakers[sortedSpeakers[i]].angles.azimuth
            <= degrees_t{ 170.0f }) {
            if (calc_2D_inv_tmatrix(speakers[sortedSpeakers[i]].angles.azimuth,
                                    speakers[sortedSpeakers[i + 1]].angles.azimuth,
                                    inv_mat[i])
                != 0) {
                exist[i] = 1;
                amount++;
            }
        }
    }

    if (degrees_t{ 360.0f } - speakers[sortedSpeakers[numSpeakers - 1]].angles.azimuth
            + speakers[sortedSpeakers[0]].angles.azimuth
        <= degrees_t{ 170 }) {
        if (calc_2D_inv_tmatrix(speakers[sortedSpeakers[numSpeakers - 1]].angles.azimuth,
                                speakers[sortedSpeakers[0]].angles.azimuth,
                                inv_mat[numSpeakers - 1])
            != 0) {
            exist[numSpeakers - 1] = 1;
        }
    }

    for (int i = 0; i < numSpeakers - 1; i++) {
        if (exist[i] == 1) {
            TripletData newTripletData;

            newTripletData.tripletSpeakerNumber[0] = sortedSpeakers[i] + 1;
            newTripletData.tripletSpeakerNumber[1] = sortedSpeakers[i + 1] + 1;
            for (int j = 0; j < 4; j++) {
                newTripletData.tripletInverseMatrix[j] = inv_mat[i][j];
            }

            triplets.push_back(newTripletData);
        }
    }

    if (exist[numSpeakers - 1] == 1) {
        TripletData newTripletData;

        newTripletData.tripletSpeakerNumber[0] = sortedSpeakers[numSpeakers - 1] + 1;
        newTripletData.tripletSpeakerNumber[1] = sortedSpeakers[0] + 1;
        for (int j = 0; j < 4; j++) {
            newTripletData.tripletInverseMatrix[j] = inv_mat[numSpeakers - 1][j];
        }

        triplets.push_back(newTripletData);
    }
}

/* Calculate volume of the parallelepiped defined by the loudspeaker
 * direction vectors and divide it with total length of the triangle sides.
 * This is used when removing too narrow triangles. */
static float vol_p_side_lgth(LoudSpeaker const & i, LoudSpeaker const & j, LoudSpeaker const & k) noexcept
{
    auto const length = i.coords.angleWith(j.coords) + i.coords.angleWith(k.coords) + j.coords.angleWith(k.coords);

    if (length <= 0.00001f) {
        return 0.0f;
    }

    auto const xProduct{ i.coords.crossProduct(j.coords) };
    auto const volper = std::abs(xProduct.dotProduct(k.coords));

    return volper / length;
}

/* Selects the loudspeaker triplets, and calculates the inversion
 * matrices for each selected triplet. A line (connection) is drawn
 * between each loudspeaker. The lines denote the sides of the
 * triangles. The triangles should not be intersecting. All crossing
 * connections are searched and the longer connection is erased.
 * This yields non-intersecting triangles, which can be used in panning.
 */
static TripletList choose_ls_triplets(std::array<LoudSpeaker, MAX_OUTPUTS> const & speakers,
                                      int const numSpeakers) noexcept
{
    jassert(numSpeakers > 0);

    /*
     * Ok, so the next part of the algorithm has to check vol_p_side_lgth() for EVERY possible speaker triplet. This
     * takes an absurd amount of time for setups bigger than 100 speakers.
     *
     * Luckily, at least two speakers have to be at a similar elevation for the triplet to be valid. Instead of looking
     * for this inside vol_p_side_lgth(), we can take advantage of this fact to reduce the search space :
     *
     * 1- Sort all the speaker indexes according to their elevation.
     *
     * 2- Select every pair of speaker that is within the maximum elevation range and for every other speaker, check for
     * vol_p_side_lgth().
     */

    // We first build an array with all the indexes
    std::vector<int> speakerIndexesSortedByElevation{};
    speakerIndexesSortedByElevation.resize(narrow<size_t>(numSpeakers));
    std::iota(std::begin(speakerIndexesSortedByElevation), std::end(speakerIndexesSortedByElevation), 0);

    // ...then we sort it according to the elevation values
    auto const sortIndexesBySpeakerElevation = [speakers](size_t const & indexA, size_t const & indexB) -> bool {
        return speakers[indexA].angles.elevation < speakers[indexB].angles.elevation;
    };
    std::sort(std::begin(speakerIndexesSortedByElevation),
              std::end(speakerIndexesSortedByElevation),
              sortIndexesBySpeakerElevation);

    // ...then we test for valid triplets ONLY when the elevation difference is within a specified range for two
    // speakers
    std::array<std::array<bool, MAX_OUTPUTS>, MAX_OUTPUTS> connections{};
    TripletList triplets{};
    for (size_t i{}; i < speakerIndexesSortedByElevation.size(); ++i) {
        auto const speaker1Index{ speakerIndexesSortedByElevation[i] };
        auto const & speaker1{ speakers[speaker1Index] };
        for (auto j{ i + 1 }; j < speakerIndexesSortedByElevation.size(); ++j) {
            auto const speaker2Index{ speakerIndexesSortedByElevation[j] };
            auto const & speaker2{ speakers[speaker2Index] };
            static constexpr degrees_t MAX_ELEVATION_DIFF{ 10.0f };
            if (speaker2.angles.elevation - speaker1.angles.elevation > MAX_ELEVATION_DIFF) {
                // The elevation difference is only going to get greater : we can move the 1st speaker and reset the
                // other loops
                break;
            }
            for (size_t k{}; k < speakerIndexesSortedByElevation.size(); ++k) {
                if (k >= i && k <= j) {
                    // If k is between i and j, it means that i and k are within the elevation threshold (as well as k
                    // and j), so they are going to get checked anyway. We also need not to include i or j twice!
                    continue;
                }
                auto const speaker3Index{ speakerIndexesSortedByElevation[k] };
                auto const & speaker3{ speakers[speaker3Index] };
                auto const isValidCandidate{ vol_p_side_lgth(speaker1, speaker2, speaker3) > MIN_VOL_P_SIDE_LENGTH };
                if (isValidCandidate) {
                    connections[speaker1Index][speaker2Index] = true;
                    connections[speaker2Index][speaker1Index] = true;
                    connections[speaker1Index][speaker3Index] = true;
                    connections[speaker3Index][speaker1Index] = true;
                    connections[speaker2Index][speaker3Index] = true;
                    connections[speaker3Index][speaker2Index] = true;

                    TripletData newTripletData{};

                    newTripletData.tripletSpeakerNumber[0] = speaker1Index;
                    newTripletData.tripletSpeakerNumber[1] = speaker2Index;
                    newTripletData.tripletSpeakerNumber[2] = speaker3Index;

                    triplets.push_back(newTripletData);
                }
            }
        }
    }

    /* Calculate distances between all lss and sorting them. */
    auto table_size = (((numSpeakers - 1) * (numSpeakers)) / 2);
    std::vector<float> distance_table{};
    distance_table.resize(table_size);
    std::fill(std::begin(distance_table), std::end(distance_table), 100000.0f);

    std::vector<int> distance_table_i{};
    distance_table_i.resize(table_size);

    std::vector<int> distance_table_j{};
    distance_table_j.resize(table_size);

    for (int i{}; i < numSpeakers; i++) {
        for (auto j = (i + 1); j < numSpeakers; j++) {
            if (connections[i][j]) {
                auto const distance = speakers[i].coords.angleWith(speakers[j].coords);
                int k{};
                while (distance_table[k] < distance) {
                    ++k;
                }
                for (auto l = (table_size - 1); l > k; l--) {
                    distance_table[l] = distance_table[l - 1];
                    distance_table_i[l] = distance_table_i[l - 1];
                    distance_table_j[l] = distance_table_j[l - 1];
                }
                distance_table[k] = distance;
                distance_table_i[k] = i;
                distance_table_j[k] = j;
            } else {
                table_size--;
            }
        }
    }

    /* Disconnecting connections which are crossing shorter ones,
     * starting from shortest one and removing all that cross it,
     * and proceeding to next shortest. */
    for (int i{}; i < table_size; ++i) {
        auto const fst_ls = distance_table_i[i];
        auto const sec_ls = distance_table_j[i];
        if (connections[fst_ls][sec_ls]) {
            for (int j{}; j < numSpeakers; j++) {
                for (auto k = j + 1; k < numSpeakers; k++) {
                    if ((j != fst_ls) && (k != sec_ls) && (k != fst_ls) && (j != sec_ls)) {
                        if (lines_intersect(fst_ls, sec_ls, j, k, speakers)) {
                            connections[j][k] = false;
                            connections[k][j] = false;
                        }
                    }
                }
            }
        }
    }

    /* Remove triangles which had crossing sides with
     * smaller triangles or include loudspeakers. */
    auto const predicate = [&connections, speakers, numSpeakers](TripletData const & triplet) -> bool {
        auto const & i = triplet.tripletSpeakerNumber[0];
        auto const & j = triplet.tripletSpeakerNumber[1];
        auto const & k = triplet.tripletSpeakerNumber[2];

        if (any_speaker_inside_triplet(i, j, k, speakers, numSpeakers)) {
            return false;
        }

        return connections[i][j] && connections[i][k] && connections[j][k];
    };
    auto const newTripletEnd{ std::partition(std::begin(triplets), std::end(triplets), predicate) };

    triplets.erase(newTripletEnd, std::end(triplets));
    triplets.shrink_to_fit();
    return triplets;
}

/* Calculates the inverse matrices for 3D.
 *
 * After this call, ls_triplets contains the speakers numbers
 * and the inverse matrix needed to compute channel gains.
 */
static void
    calculate_3x3_matrixes(TripletList & triplets, std::array<LoudSpeaker, MAX_OUTPUTS> & speakers, int /*numSpeakers*/)
{
    for (auto & triplet : triplets) {
        auto const * lp1 = &(speakers[triplet.tripletSpeakerNumber[0]].coords);
        auto const * lp2 = &(speakers[triplet.tripletSpeakerNumber[1]].coords);
        auto const * lp3 = &(speakers[triplet.tripletSpeakerNumber[2]].coords);

        /* Matrix inversion. */
        auto * inverseMatrix = triplet.tripletInverseMatrix;
        auto const inverseDet
            = 1.0f
              / (lp1->x * ((lp2->y * lp3->z) - (lp2->z * lp3->y)) - lp1->y * ((lp2->x * lp3->z) - (lp2->z * lp3->x))
                 + lp1->z * ((lp2->x * lp3->y) - (lp2->y * lp3->x)));

        inverseMatrix[0] = ((lp2->y * lp3->z) - (lp2->z * lp3->y)) * inverseDet;
        inverseMatrix[3] = ((lp1->y * lp3->z) - (lp1->z * lp3->y)) * -inverseDet;
        inverseMatrix[6] = ((lp1->y * lp2->z) - (lp1->z * lp2->y)) * inverseDet;
        inverseMatrix[1] = ((lp2->x * lp3->z) - (lp2->z * lp3->x)) * -inverseDet;
        inverseMatrix[4] = ((lp1->x * lp3->z) - (lp1->z * lp3->x)) * inverseDet;
        inverseMatrix[7] = ((lp1->x * lp2->z) - (lp1->z * lp2->x)) * -inverseDet;
        inverseMatrix[2] = ((lp2->x * lp3->y) - (lp2->y * lp3->x)) * inverseDet;
        inverseMatrix[5] = ((lp1->x * lp3->y) - (lp1->y * lp3->x)) * -inverseDet;
        inverseMatrix[8] = ((lp1->x * lp2->y) - (lp1->y * lp2->x)) * inverseDet;
    }
}

VbapData * init_vbap_from_speakers(std::array<LoudSpeaker, MAX_OUTPUTS> & speakers,
                                   int const count,
                                   int const dimensions,
                                   std::array<output_patch_t, MAX_OUTPUTS> const & outputPatches,
                                   output_patch_t const maxOutputPatch)
{
    int offset{};
    TripletList triplets{};
    auto * data = new VbapData;
    if (dimensions == 3) {
        triplets = choose_ls_triplets(speakers, count);
        calculate_3x3_matrixes(triplets, speakers, count);
        offset = 1;
    } else if (dimensions == 2) {
        choose_ls_tuplets(speakers, triplets, count);
    }

    data->numOutputPatches = count;
    for (int i = 0; i < count; i++) {
        data->outputPatches[i] = outputPatches[i];
    }

    data->dimension = dimensions;
    data->numSpeakers = speakers.size();

    for (auto const & triplet : triplets) {
        SpeakerSet newSet{};
        for (int j = 0; j < data->dimension; j++) {
            newSet.speakerNos[j] = outputPatches[triplet.tripletSpeakerNumber[j] + offset - 1].get();
        }
        for (int j = 0; j < (data->dimension * data->dimension); j++) {
            newSet.invMx[j] = triplet.tripletInverseMatrix[j];
        }
        data->speakerSets.add(newSet);
    }

    return data;
}

void vbap2(SourceData const & source, SpeakersSpatGains & gains, VbapData & data) noexcept
{
    int i;
    jassert(source.position);
    jassert(source.vector);
    data.angularDirection.azimuth = source.vector->azimuth;
    data.angularDirection.elevation = source.vector->elevation;
    data.angularDirection.length = 1.0;
    data.cartesianDirection = *source.position;
    std::fill(gains.begin(), gains.end(), 0.0f);
    compute_gains(data.speakerSets, gains, data.numSpeakers, data.cartesianDirection, data.dimension);
    if (data.dimension == 3) {
        if (source.azimuthSpan > 0 || source.zenithSpan > 0) {
            spreadit_azi_ele(source, gains, data);
        }
    } else {
        if (source.azimuthSpan > 0) {
            spreadit_azi(source, gains, data);
        }
    }
}

juce::Array<Triplet> vbap_get_triplets(VbapData const & data)
{
    juce::Array<Triplet> result{};
    result.ensureStorageAllocated(data.speakerSets.size());
    for (auto const & set : data.speakerSets) {
        Triplet triplet{};

        // triplet.id1 = data.outputPatches[set.speakerNos[0] - 1];
        // triplet.id2 = data.outputPatches[set.speakerNos[1] - 1];
        // triplet.id3 = data.outputPatches[set.speakerNos[2] - 1];
        triplet.id1 = output_patch_t{ set.speakerNos[0] };
        triplet.id2 = output_patch_t{ set.speakerNos[1] };
        triplet.id3 = output_patch_t{ set.speakerNos[2] };

        jassert(LEGAL_OUTPUT_PATCH_RANGE.contains(triplet.id1) && LEGAL_OUTPUT_PATCH_RANGE.contains(triplet.id2)
                && LEGAL_OUTPUT_PATCH_RANGE.contains(triplet.id3));

        result.add(triplet);
    }
    return result;
}