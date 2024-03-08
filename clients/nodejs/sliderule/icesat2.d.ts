import { Emitter } from 'mitt';

// Define the types for the constants
export const CNF_POSSIBLE_TEP: number;
export const CNF_NOT_CONSIDERED: number;
export const CNF_BACKGROUND: number;
export const CNF_WITHIN_10M: number;
export const CNF_SURFACE_LOW: number;
export const CNF_SURFACE_MEDIUM: number;
export const CNF_SURFACE_HIGH: number;
export const SRT_LAND: number;
export const SRT_OCEAN: number;
export const SRT_SEA_ICE: number;
export const SRT_LAND_ICE: number;
export const SRT_INLAND_WATER: number;
export const MAX_COORDS_IN_POLYGON: number;
export const GT1L: number;
export const GT1R: number;
export const GT2L: number;
export const GT2R: number;
export const GT3L: number;
export const GT3R: number;
export const STRONG_SPOTS: number[];
export const WEAK_SPOTS: number[];
export const LEFT_PAIR: number;
export const RIGHT_PAIR: number;
export const SC_BACKWARD: number;
export const SC_FORWARD: number;
export const ATL08_WATER: number;
export const ATL08_LAND: number;
export const ATL08_SNOW: number;
export const ATL08_ICE: number;

// Define the P constant as an object with string keys and number values
export const P: { [key: string]: number };

  // Define the parameter type for the atl06p function
  interface Atl06pParams {
    asset?: string;
    [key: string]: any; // Other dynamic keys
}
type Resource = any; // Replace 'any' with the actual type of a resource

// Define the callback type
interface Callbacks {
    atl06rec?: (result: any) => void;
    [key: string]: ((result: any) => void) | undefined;
}

// Define the function atl06p
export function atl06p(
    parm: Atl06pParams,
    resources: Resource[], 
    callbacks?: Callbacks | null
): Promise<any[]> | void; 
