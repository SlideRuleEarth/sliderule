// Define the constants
export const ALL_ROWS: number;

// Define type for datatypes
export const datatypes: {
  TEXT: number;
  REAL: number;
  INTEGER: number;
  DYNAMIC: number;
};

// Define type for the h5 function
export function h5(
  dataset: string,
  resource: string,
  asset: string,
  datatype?: number,
  col?: number,
  startrow?: number,
  numrows?: number,
  callbacks?: { [key: string]: (...args: any[]) => void } | null
): Promise<any> | void; // Replace 'any' with a more specific return type if possible
